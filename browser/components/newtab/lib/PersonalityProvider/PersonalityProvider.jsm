/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  Utils: "resource://services-settings/Utils.sys.mjs",
});

const { BasePromiseWorker } = ChromeUtils.importESModule(
  "resource://gre/modules/PromiseWorker.sys.mjs"
);

const RECIPE_NAME = "personality-provider-recipe";
const MODELS_NAME = "personality-provider-models";

class PersonalityProvider {
  constructor(modelKeys) {
    this.modelKeys = modelKeys;
    this.onSync = this.onSync.bind(this);
    this.setup();
  }

  setScores(scores) {
    this.scores = scores || {};
    this.interestConfig = this.scores.interestConfig;
    this.interestVector = this.scores.interestVector;
  }

  get personalityProviderWorker() {
    if (this._personalityProviderWorker) {
      return this._personalityProviderWorker;
    }

    this._personalityProviderWorker = new BasePromiseWorker(
      "resource://activity-stream/lib/PersonalityProvider/PersonalityProvider.worker.mjs",
      { type: "module" }
    );

    return this._personalityProviderWorker;
  }

  get baseAttachmentsURL() {
    // Returning a promise, so we can have an async getter.
    return this._getBaseAttachmentsURL();
  }

  async _getBaseAttachmentsURL() {
    if (this._baseAttachmentsURL) {
      return this._baseAttachmentsURL;
    }
    const server = lazy.Utils.SERVER_URL;
    const serverInfo = await (
      await fetch(`${server}/`, {
        credentials: "omit",
      })
    ).json();
    const {
      capabilities: {
        attachments: { base_url },
      },
    } = serverInfo;
    this._baseAttachmentsURL = base_url;
    return this._baseAttachmentsURL;
  }

  setup() {
    this.setupSyncAttachment(RECIPE_NAME);
    this.setupSyncAttachment(MODELS_NAME);
  }

  teardown() {
    this.teardownSyncAttachment(RECIPE_NAME);
    this.teardownSyncAttachment(MODELS_NAME);
    if (this._personalityProviderWorker) {
      this._personalityProviderWorker.terminate();
    }
  }

  setupSyncAttachment(collection) {
    lazy.RemoteSettings(collection).on("sync", this.onSync);
  }

  teardownSyncAttachment(collection) {
    lazy.RemoteSettings(collection).off("sync", this.onSync);
  }

  onSync(event) {
    this.personalityProviderWorker.post("onSync", [event]);
  }

  /**
   * Gets contents of the attachment if it already exists on file,
   * and if not attempts to download it.
   */
  getAttachment(record) {
    return this.personalityProviderWorker.post("getAttachment", [record]);
  }

  /**
   * Returns a Recipe from remote settings to be consumed by a RecipeExecutor.
   * A Recipe is a set of instructions on how to processes a RecipeExecutor.
   */
  async getRecipe() {
    if (!this.recipes || !this.recipes.length) {
      const result = await lazy.RemoteSettings(RECIPE_NAME).get();
      this.recipes = await Promise.all(
        result.map(async record => ({
          ...(await this.getAttachment(record)),
          recordKey: record.key,
        }))
      );
    }
    return this.recipes[0];
  }

  /**
   * Grabs a slice of browse history for building a interest vector
   */
  async fetchHistory(columns, beginTimeSecs, endTimeSecs) {
    let sql = `SELECT url, title, visit_count, frecency, last_visit_date, description
    FROM moz_places
    WHERE last_visit_date >= ${beginTimeSecs * 1000000}
    AND last_visit_date < ${endTimeSecs * 1000000}`;
    columns.forEach(requiredColumn => {
      sql += ` AND IFNULL(${requiredColumn}, '') <> ''`;
    });
    sql += " LIMIT 30000";

    const { activityStreamProvider } = lazy.NewTabUtils;
    const history = await activityStreamProvider.executePlacesQuery(sql, {
      columns,
      params: {},
    });

    return history;
  }

  /**
   * Handles setup and metrics of history fetch.
   */
  async getHistory() {
    let endTimeSecs = new Date().getTime() / 1000;
    let beginTimeSecs = endTimeSecs - this.interestConfig.history_limit_secs;
    if (
      !this.interestConfig ||
      !this.interestConfig.history_required_fields ||
      !this.interestConfig.history_required_fields.length
    ) {
      return [];
    }
    let history = await this.fetchHistory(
      this.interestConfig.history_required_fields,
      beginTimeSecs,
      endTimeSecs
    );

    return history;
  }

  async setBaseAttachmentsURL() {
    await this.personalityProviderWorker.post("setBaseAttachmentsURL", [
      await this.baseAttachmentsURL,
    ]);
  }

  async setInterestConfig() {
    this.interestConfig = this.interestConfig || (await this.getRecipe());
    await this.personalityProviderWorker.post("setInterestConfig", [
      this.interestConfig,
    ]);
  }

  async setInterestVector() {
    await this.personalityProviderWorker.post("setInterestVector", [
      this.interestVector,
    ]);
  }

  async fetchModels() {
    const models = await lazy.RemoteSettings(MODELS_NAME).get();
    return this.personalityProviderWorker.post("fetchModels", [models]);
  }

  async generateTaggers() {
    await this.personalityProviderWorker.post("generateTaggers", [
      this.modelKeys,
    ]);
  }

  async generateRecipeExecutor() {
    await this.personalityProviderWorker.post("generateRecipeExecutor");
  }

  async createInterestVector() {
    const history = await this.getHistory();

    const interestVectorResult = await this.personalityProviderWorker.post(
      "createInterestVector",
      [history]
    );

    return interestVectorResult;
  }

  async init(callback) {
    await this.setBaseAttachmentsURL();
    await this.setInterestConfig();
    if (!this.interestConfig) {
      return;
    }

    // We always generate a recipe executor, no cache used here.
    // This is because the result of this is an object with
    // functions (taggers) so storing it in cache is not possible.
    // Thus we cannot use it to rehydrate anything.
    const fetchModelsResult = await this.fetchModels();
    // If this fails, log an error and return.
    if (!fetchModelsResult.ok) {
      return;
    }
    await this.generateTaggers();
    await this.generateRecipeExecutor();

    // If we don't have a cached vector, create a new one.
    if (!this.interestVector) {
      const interestVectorResult = await this.createInterestVector();
      // If that failed, log an error and return.
      if (!interestVectorResult.ok) {
        return;
      }
      this.interestVector = interestVectorResult.interestVector;
    }

    // This happens outside the createInterestVector call above,
    // because create can be skipped if rehydrating from cache.
    // In that case, the interest vector is provided and not created, so we just set it.
    await this.setInterestVector();

    this.initialized = true;
    if (callback) {
      callback();
    }
  }

  async calculateItemRelevanceScore(pocketItem) {
    if (!this.initialized) {
      return pocketItem.item_score || 1;
    }
    const itemRelevanceScore = await this.personalityProviderWorker.post(
      "calculateItemRelevanceScore",
      [pocketItem]
    );
    if (!itemRelevanceScore) {
      return -1;
    }
    const { scorableItem, rankingVector } = itemRelevanceScore;
    // Put the results on the item for debugging purposes.
    pocketItem.scorableItem = scorableItem;
    pocketItem.rankingVector = rankingVector;
    return rankingVector.score;
  }

  /**
   * Returns an object holding the personalization scores of this provider instance.
   */
  getScores() {
    return {
      // We cannot return taggers here.
      // What we return here goes into persistent cache, and taggers have functions on it.
      // If we attempted to save taggers into persistent cache, it would store it to disk,
      // and the next time we load it, it would start thowing function is not defined.
      interestConfig: this.interestConfig,
      interestVector: this.interestVector,
    };
  }
}

const EXPORTED_SYMBOLS = ["PersonalityProvider"];
