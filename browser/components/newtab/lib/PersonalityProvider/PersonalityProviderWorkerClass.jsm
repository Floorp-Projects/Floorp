/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We load this into a worker using importScripts, and in tests using import.
// We use var to avoid name collision errors.
// eslint-disable-next-line no-var
var EXPORTED_SYMBOLS = ["PersonalityProviderWorker"];

const PERSONALITY_PROVIDER_DIR = OS.Path.join(
  OS.Constants.Path.localProfileDir,
  "personality-provider"
);

/**
 * V2 provider builds and ranks an interest profile (also called an “interest vector”) off the browse history.
 * This allows Firefox to classify pages into topics, by examining the text found on the page.
 * It does this by looking at the history text content, title, and description.
 */
const PersonalityProviderWorker = class PersonalityProviderWorker {
  // A helper function to read and decode a file, it isn't a stand alone function.
  // If you use this, ensure you check the file exists and you have a try catch.
  _getFileStr(filepath) {
    const binaryData = OS.File.read(filepath);
    const gTextDecoder = new TextDecoder();
    return gTextDecoder.decode(binaryData);
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
    toRemove.map(record => this.deleteAttachment(record));

    // Download every new/updated attachment.
    const toDownload = created.concat(updated.map(u => u.new));
    toDownload.map(record => this.maybeDownloadAttachment(record));
  }

  /**
   * Attempts to download the attachment, but only if it doesn't already exist.
   */
  maybeDownloadAttachment(record, retries = 3) {
    const {
      attachment: { filename, size },
    } = record;
    OS.File.makeDir(PERSONALITY_PROVIDER_DIR);
    const localFilePath = OS.Path.join(PERSONALITY_PROVIDER_DIR, filename);

    let retry = 0;
    while (
      retry++ < retries &&
      // exists is an issue for perf because I might not need to call it.
      (!OS.File.exists(localFilePath) ||
        OS.File.stat(localFilePath).size !== size)
    ) {
      this._downloadAttachment(record);
    }
  }

  /**
   * Downloads the attachment to disk assuming the dir already exists
   * and any existing files matching the filename are clobbered.
   */
  _downloadAttachment(record) {
    const {
      attachment: { location, filename },
    } = record;
    const remoteFilePath = this.baseAttachmentsURL + location;
    const localFilePath = OS.Path.join(PERSONALITY_PROVIDER_DIR, filename);

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

    OS.File.writeAtomic(localFilePath, bytes, {
      tmpPath: `${localFilePath}.tmp`,
    });
  }

  deleteAttachment(record) {
    const {
      attachment: { filename },
    } = record;
    OS.File.makeDir(PERSONALITY_PROVIDER_DIR);
    const path = OS.Path.join(PERSONALITY_PROVIDER_DIR, filename);

    OS.File.remove(path, { ignoreAbsent: true });
    OS.File.removeEmptyDir(PERSONALITY_PROVIDER_DIR, {
      ignoreAbsent: true,
    });
  }

  /**
   * Gets contents of the attachment if it already exists on file,
   * and if not attempts to download it.
   */
  getAttachment(record) {
    const {
      attachment: { filename },
    } = record;
    const filepath = OS.Path.join(PERSONALITY_PROVIDER_DIR, filename);

    try {
      this.maybeDownloadAttachment(record);
      return JSON.parse(this._getFileStr(filepath));
    } catch (error) {
      console.error(`Failed to load ${filepath}: ${error.message}`);
    }
    return {};
  }

  fetchModels(models) {
    this.models = models.map(record => ({
      ...this.getAttachment(record),
      recordKey: record.key,
    }));
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
          // eslint-disable-next-line no-undef
          nbTaggers.push(new NaiveBayesTextTagger(model, toksToTfIdfVector));
        } else if (model.model_type === "nmf") {
          // eslint-disable-next-line no-undef
          nmfTaggers[model.parent_tag] = new NmfTextTagger(
            model,
            // eslint-disable-next-line no-undef
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
    // eslint-disable-next-line no-undef
    const recipeExecutor = new RecipeExecutor(
      this.taggers.nbTaggers,
      this.taggers.nmfTaggers,
      // eslint-disable-next-line no-undef
      tokenize
    );
    this.recipeExecutor = recipeExecutor;
  }

  /**
   * Examines the user's browse history and returns an interest vector that
   * describes the topics the user frequently browses.
   */
  createInterestVector(history) {
    let interestVector = {};

    for (let historyRec of history) {
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
    let scorableItem = this.recipeExecutor.executeRecipe(
      pocketItem,
      this.interestConfig.item_to_rank_builder
    );
    if (scorableItem === null) {
      return -1;
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
      return -1;
    }
    return rankingVector.score;
  }
};
