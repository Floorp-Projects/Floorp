/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  tokenize,
  toksToTfIdfVector,
} from "resource://activity-stream/lib/PersonalityProvider/Tokenize.mjs";
import { NaiveBayesTextTagger } from "resource://activity-stream/lib/PersonalityProvider/NaiveBayesTextTagger.mjs";
import { NmfTextTagger } from "resource://activity-stream/lib/PersonalityProvider/NmfTextTagger.mjs";
import { RecipeExecutor } from "resource://activity-stream/lib/PersonalityProvider/RecipeExecutor.mjs";

// A helper function to create a hash out of a file.
async function _getFileHash(filepath) {
  const data = await IOUtils.read(filepath);
  // File is an instance of Uint8Array
  const digest = await crypto.subtle.digest("SHA-256", data);
  const uint8 = new Uint8Array(digest);
  // return the two-digit hexadecimal code for a byte
  const toHex = b => b.toString(16).padStart(2, "0");
  return Array.from(uint8, toHex).join("");
}

/**
 * V2 provider builds and ranks an interest profile (also called an “interest vector”) off the browse history.
 * This allows Firefox to classify pages into topics, by examining the text found on the page.
 * It does this by looking at the history text content, title, and description.
 */
export class PersonalityProviderWorker {
  async getPersonalityProviderDir() {
    const personalityProviderDir = PathUtils.join(
      await PathUtils.getLocalProfileDir(),
      "personality-provider"
    );

    // Cache this so we don't need to await again.
    this.getPersonalityProviderDir = () =>
      Promise.resolve(personalityProviderDir);
    return personalityProviderDir;
  }

  setBaseAttachmentsURL(url) {
    this.baseAttachmentsURL = url;
  }

  setInterestConfig(interestConfig) {
    this.interestConfig = interestConfig;
  }

  setInterestVector(interestVector) {
    this.interestVector = interestVector;
  }

  onSync(event) {
    const {
      data: { created, updated, deleted },
    } = event;
    // Remove every removed attachment.
    const toRemove = deleted.concat(updated.map(u => u.old));
    toRemove.forEach(record => this.deleteAttachment(record));

    // Download every new/updated attachment.
    const toDownload = created.concat(updated.map(u => u.new));
    // maybeDownloadAttachment is async but we don't care inside onSync.
    toDownload.forEach(record => this.maybeDownloadAttachment(record));
  }

  /**
   * Attempts to download the attachment, but only if it doesn't already exist.
   */
  async maybeDownloadAttachment(record, retries = 3) {
    const {
      attachment: { filename, hash, size },
    } = record;
    await IOUtils.makeDirectory(await this.getPersonalityProviderDir());
    const localFilePath = PathUtils.join(
      await this.getPersonalityProviderDir(),
      filename
    );

    let retry = 0;
    while (
      retry++ < retries &&
      // exists is an issue for perf because I might not need to call it.
      (!(await IOUtils.exists(localFilePath)) ||
        (await IOUtils.stat(localFilePath)).size !== size ||
        (await _getFileHash(localFilePath)) !== hash)
    ) {
      await this._downloadAttachment(record);
    }
  }

  /**
   * Downloads the attachment to disk assuming the dir already exists
   * and any existing files matching the filename are clobbered.
   */
  async _downloadAttachment(record) {
    const {
      attachment: { location: loc, filename },
    } = record;
    const remoteFilePath = this.baseAttachmentsURL + loc;
    const localFilePath = PathUtils.join(
      await this.getPersonalityProviderDir(),
      filename
    );

    const xhr = new XMLHttpRequest();
    // Set false here for a synchronous request, because we're in a worker.
    xhr.open("GET", remoteFilePath, false);
    xhr.setRequestHeader("Accept-Encoding", "gzip");
    xhr.responseType = "arraybuffer";
    xhr.withCredentials = false;
    xhr.send(null);

    if (xhr.status !== 200) {
      console.error(`Failed to fetch ${remoteFilePath}: ${xhr.statusText}`);
      return;
    }

    const buffer = xhr.response;
    const bytes = new Uint8Array(buffer);

    await IOUtils.write(localFilePath, bytes, {
      tmpPath: `${localFilePath}.tmp`,
    });
  }

  async deleteAttachment(record) {
    const {
      attachment: { filename },
    } = record;
    await IOUtils.makeDirectory(await this.getPersonalityProviderDir());
    const path = PathUtils.join(
      await this.getPersonalityProviderDir(),
      filename
    );

    await IOUtils.remove(path, { ignoreAbsent: true });
    // Cleanup the directory if it is empty, do nothing if it is not empty.
    try {
      await IOUtils.remove(await this.getPersonalityProviderDir(), {
        ignoreAbsent: true,
      });
    } catch (e) {
      // This is likely because the directory is not empty, so we don't care.
    }
  }

  /**
   * Gets contents of the attachment if it already exists on file,
   * and if not attempts to download it.
   */
  async getAttachment(record) {
    const {
      attachment: { filename },
    } = record;
    const filepath = PathUtils.join(
      await this.getPersonalityProviderDir(),
      filename
    );

    try {
      await this.maybeDownloadAttachment(record);
      return await IOUtils.readJSON(filepath);
    } catch (error) {
      console.error(`Failed to load ${filepath}: ${error.message}`);
    }
    return {};
  }

  async fetchModels(models) {
    this.models = await Promise.all(
      models.map(async record => ({
        ...(await this.getAttachment(record)),
        recordKey: record.key,
      }))
    );
    if (!this.models.length) {
      return {
        ok: false,
      };
    }
    return {
      ok: true,
    };
  }

  generateTaggers(modelKeys) {
    if (!this.taggers) {
      let nbTaggers = [];
      let nmfTaggers = {};

      for (let model of this.models) {
        if (!modelKeys.includes(model.recordKey)) {
          continue;
        }
        if (model.model_type === "nb") {
          nbTaggers.push(new NaiveBayesTextTagger(model, toksToTfIdfVector));
        } else if (model.model_type === "nmf") {
          nmfTaggers[model.parent_tag] = new NmfTextTagger(
            model,
            toksToTfIdfVector
          );
        }
      }
      this.taggers = { nbTaggers, nmfTaggers };
    }
  }

  /**
   * Sets and generates a Recipe Executor.
   * A Recipe Executor is a set of actions that can be consumed by a Recipe.
   * The Recipe determines the order and specifics of which the actions are called.
   */
  generateRecipeExecutor() {
    const recipeExecutor = new RecipeExecutor(
      this.taggers.nbTaggers,
      this.taggers.nmfTaggers,
      tokenize
    );
    this.recipeExecutor = recipeExecutor;
  }

  /**
   * Examines the user's browse history and returns an interest vector that
   * describes the topics the user frequently browses.
   */
  createInterestVector(historyObj) {
    let interestVector = {};

    for (let historyRec of historyObj) {
      let ivItem = this.recipeExecutor.executeRecipe(
        historyRec,
        this.interestConfig.history_item_builder
      );
      if (ivItem === null) {
        continue;
      }
      interestVector = this.recipeExecutor.executeCombinerRecipe(
        interestVector,
        ivItem,
        this.interestConfig.interest_combiner
      );
      if (interestVector === null) {
        return null;
      }
    }

    const finalResult = this.recipeExecutor.executeRecipe(
      interestVector,
      this.interestConfig.interest_finalizer
    );

    return {
      ok: true,
      interestVector: finalResult,
    };
  }

  /**
   * Calculates a score of a Pocket item when compared to the user's interest
   * vector. Returns the score. Higher scores are better. Assumes this.interestVector
   * is populated.
   */
  calculateItemRelevanceScore(pocketItem) {
    const { personalization_models } = pocketItem;
    let scorableItem;

    // If the server provides some models, we can just use them,
    // and skip generating them.
    if (personalization_models && Object.keys(personalization_models).length) {
      scorableItem = {
        id: pocketItem.id,
        item_tags: personalization_models,
        item_score: pocketItem.item_score,
        item_sort_id: 1,
      };
    } else {
      scorableItem = this.recipeExecutor.executeRecipe(
        pocketItem,
        this.interestConfig.item_to_rank_builder
      );
      if (scorableItem === null) {
        return null;
      }
    }

    // We're doing a deep copy on an object.
    let rankingVector = JSON.parse(JSON.stringify(this.interestVector));

    Object.keys(scorableItem).forEach(key => {
      rankingVector[key] = scorableItem[key];
    });

    rankingVector = this.recipeExecutor.executeRecipe(
      rankingVector,
      this.interestConfig.item_ranker
    );

    if (rankingVector === null) {
      return null;
    }

    return { scorableItem, rankingVector };
  }
}
