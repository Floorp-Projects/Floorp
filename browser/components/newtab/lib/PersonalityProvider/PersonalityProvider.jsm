/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "RemoteSettings",
  "resource://services-settings/remote-settings.js"
);

const { actionCreators: ac } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const { BasePromiseWorker } = ChromeUtils.import(
  "resource://gre/modules/PromiseWorker.jsm"
);

const RECIPE_NAME = "personality-provider-recipe";
const MODELS_NAME = "personality-provider-models";

this.PersonalityProvider = class PersonalityProvider {
  constructor(
    timeSegments,
    parameterSets,
    maxHistoryQueryResults,
    version,
    scores,
    v2Params
  ) {
    this.timeSegments = timeSegments;
    this.parameterSets = parameterSets;
    this.v2Params = v2Params || {};
    this.modelKeys = this.v2Params.modelKeys;
    this.dispatch = this.v2Params.dispatch;
    if (!this.dispatch) {
      this.dispatch = () => {};
    }
    this.maxHistoryQueryResults = maxHistoryQueryResults;
    this.version = version;
    this.scores = scores || {};
    this.interestConfig = this.scores.interestConfig;
    this.interestVector = this.scores.interestVector;
    this.onSync = this.onSync.bind(this);
    this.setup();
  }

  get personalityProviderWorker() {
    if (this._personalityProviderWorker) {
      return this._personalityProviderWorker;
    }

    this._personalityProviderWorker = new BasePromiseWorker(
      "resource://activity-stream/lib/PersonalityProvider/PersonalityProviderWorker.js"
    );

    // As the PersonalityProviderWorker performs I/O, we can receive instances of
    // OS.File.Error, so we need to install a decoder.
    this._personalityProviderWorker.ExceptionHandlers["OS.File.Error"] =
      OS.File.Error.fromMsg;
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
    const server = Services.prefs.getCharPref("services.settings.server");
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
  }

  setupSyncAttachment(collection) {
    RemoteSettings(collection).on("sync", this.onSync);
  }

  teardownSyncAttachment(collection) {
    RemoteSettings(collection).off("sync", this.onSync);
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
      const start = Cu.now();
      const result = await RemoteSettings(RECIPE_NAME).get();
      this.recipes = await Promise.all(
        result.map(async record => ({
          ...(await this.getAttachment(record)),
          recordKey: record.key,
        }))
      );
      this.dispatch(
        ac.PerfEvent({
          event: "PERSONALIZATION_V2_GET_RECIPE_DURATION",
          value: Math.round(Cu.now() - start),
        })
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

    const { activityStreamProvider } = NewTabUtils;
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

    this.dispatch(
      ac.PerfEvent({
        event: "PERSONALIZATION_V2_HISTORY_SIZE",
        value: history.length,
      })
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
    const models = await RemoteSettings(MODELS_NAME).get();
    return this.personalityProviderWorker.post("fetchModels", [models]);
  }

  async generateTaggers() {
    const start = Cu.now();
    await this.personalityProviderWorker.post("generateTaggers", [
      this.modelKeys,
    ]);
    this.dispatch(
      ac.PerfEvent({
        event: "PERSONALIZATION_V2_TAGGERS_DURATION",
        value: Math.round(Cu.now() - start),
      })
    );
  }

  async generateRecipeExecutor() {
    const start = Cu.now();
    await this.personalityProviderWorker.post("generateRecipeExecutor");
    this.dispatch(
      ac.PerfEvent({
        event: "PERSONALIZATION_V2_RECIPE_EXECUTOR_DURATION",
        value: Math.round(Cu.now() - start),
      })
    );
  }

  async createInterestVector() {
    const history = await this.getHistory();

    const interestVectorPerfStart = Cu.now();
    const interestVectorResult = await this.personalityProviderWorker.post(
      "createInterestVector",
      [history]
    );

    this.dispatch(
      ac.PerfEvent({
        event: "PERSONALIZATION_V2_CREATE_INTEREST_VECTOR_DURATION",
        value: Math.round(Cu.now() - interestVectorPerfStart),
      })
    );
    return interestVectorResult;
  }

  async init(callback) {
    const perfStart = Cu.now();
    await this.setBaseAttachmentsURL();
    await this.setInterestConfig();
    if (!this.interestConfig) {
      this.dispatch(
        ac.PerfEvent({ event: "PERSONALIZATION_V2_GET_RECIPE_ERROR" })
      );
      return;
    }

    // We always generate a recipe executor, no cache used here.
    // This is because the result of this is an object with
    // functions (taggers) so storing it in cache is not possible.
    // Thus we cannot use it to rehydrate anything.
    const fetchModelsResult = await this.fetchModels();
    // If this fails, log an error and return.
    if (!fetchModelsResult.ok) {
      this.dispatch(
        ac.PerfEvent({
          event: "PERSONALIZATION_V2_FETCH_MODELS_ERROR",
        })
      );
      return;
    }
    await this.generateTaggers();
    await this.generateRecipeExecutor();

    // If we don't have a cached vector, create a new one.
    if (!this.interestVector) {
      const interestVectorResult = await this.createInterestVector();
      // If that failed, log an error and return.
      if (!interestVectorResult.ok) {
        this.dispatch(
          ac.PerfEvent({
            event: "PERSONALIZATION_V2_CREATE_INTEREST_VECTOR_ERROR",
          })
        );
        return;
      }
      this.interestVector = interestVectorResult.interestVector;
    }

    // This happens outside the createInterestVector call above,
    // because create can be skipped if rehydrating from cache.
    // In that case, the interest vector is provided and not created, so we just set it.
    await this.setInterestVector();

    this.dispatch(
      ac.PerfEvent({
        event: "PERSONALIZATION_V2_TOTAL_DURATION",
        value: Math.round(Cu.now() - perfStart),
      })
    );

    this.initialized = true;
    if (callback) {
      callback();
    }
  }

  dispatchRelevanceScoreDuration(start) {
    // If v2 is not yet initialized we don't bother tracking yet.
    // Before it is initialized it doesn't do any ranking.
    // Once it's initialized it ensures ranking is done.
    // v1 doesn't have any initialized issues around ranking,
    // and should be ready right away.
    if (this.initialized) {
      this.dispatch(
        ac.PerfEvent({
          event: "PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION",
          value: Math.round(Cu.now() - start),
        })
      );
    }
  }

  calculateItemRelevanceScore(pocketItem) {
    if (!this.initialized) {
      return pocketItem.item_score || 1;
    }
    return this.personalityProviderWorker.post("calculateItemRelevanceScore", [
      pocketItem,
    ]);
  }

  /**
   * Returns an object holding the settings and affinity scores of this provider instance.
   */
  getAffinities() {
    return {
      timeSegments: this.timeSegments,
      parameterSets: this.parameterSets,
      maxHistoryQueryResults: this.maxHistoryQueryResults,
      version: this.version,
      scores: {
        // We cannot return taggers here.
        // What we return here goes into persistent cache, and taggers have functions on it.
        // If we attempted to save taggers into persistent cache, it would store it to disk,
        // and the next time we load it, it would start thowing function is not defined.
        interestConfig: this.interestConfig,
        interestVector: this.interestVector,
      },
    };
  }
};

const EXPORTED_SYMBOLS = ["PersonalityProvider"];
