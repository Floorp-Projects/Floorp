/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "timerManager",
                                   "@mozilla.org/updates/timer-manager;1",
                                   "nsIUpdateTimerManager");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences", "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Storage",
                                  "resource://shield-recipe-client/lib/Storage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NormandyDriver",
                                  "resource://shield-recipe-client/lib/NormandyDriver.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FilterExpressions",
                                  "resource://shield-recipe-client/lib/FilterExpressions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NormandyApi",
                                  "resource://shield-recipe-client/lib/NormandyApi.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SandboxManager",
                                  "resource://shield-recipe-client/lib/SandboxManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ClientEnvironment",
                                  "resource://shield-recipe-client/lib/ClientEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CleanupManager",
                                  "resource://shield-recipe-client/lib/CleanupManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ActionSandboxManager",
                                  "resource://shield-recipe-client/lib/ActionSandboxManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonStudies",
                                  "resource://shield-recipe-client/lib/AddonStudies.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Uptake",
                                  "resource://shield-recipe-client/lib/Uptake.jsm");

Cu.importGlobalProperties(["fetch"]);

this.EXPORTED_SYMBOLS = ["RecipeRunner"];

const log = LogManager.getLogger("recipe-runner");
const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");
const TIMER_NAME = "recipe-client-addon-run";
const RUN_INTERVAL_PREF = "run_interval_seconds";
const FIRST_RUN_PREF = "first_run";
const PREF_CHANGED_TOPIC = "nsPref:changed";

this.RecipeRunner = {
  async init() {
    if (!this.checkPrefs()) {
      return;
    }

    // Run immediately on first run, or if dev mode is enabled.
    if (prefs.getBoolPref(FIRST_RUN_PREF) || prefs.getBoolPref("dev_mode")) {
      await this.run();
      prefs.setBoolPref(FIRST_RUN_PREF, false);
    }

    this.registerTimer();
  },

  registerTimer() {
    this.updateRunInterval();
    CleanupManager.addCleanupHandler(() => timerManager.unregisterTimer(TIMER_NAME));

    // Watch for the run interval to change, and re-register the timer with the new value
    prefs.addObserver(RUN_INTERVAL_PREF, this);
    CleanupManager.addCleanupHandler(() => prefs.removeObserver(RUN_INTERVAL_PREF, this));
  },

  checkPrefs() {
    // Only run if Unified Telemetry is enabled.
    if (!Services.prefs.getBoolPref("toolkit.telemetry.unified")) {
      log.info("Disabling RecipeRunner because Unified Telemetry is disabled.");
      return false;
    }

    if (!prefs.getBoolPref("enabled")) {
      log.info("Recipe Client is disabled.");
      return false;
    }

    const apiUrl = prefs.getCharPref("api_url");
    if (!apiUrl || !apiUrl.startsWith("https://")) {
      log.error(`Non HTTPS URL provided for extensions.shield-recipe-client.api_url: ${apiUrl}`);
      return false;
    }

    return true;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case PREF_CHANGED_TOPIC:
        this.observePrefChange(data);
        break;
    }
  },

  /**
   * Watch for preference changes from Services.pref.addObserver.
   */
  observePrefChange(prefName) {
    if (prefName === RUN_INTERVAL_PREF) {
      this.updateRunInterval();
    } else {
      log.debug(`Observer fired with unexpected pref change: ${prefName}`);
    }
  },

  updateRunInterval() {
    // Run once every `runInterval` wall-clock seconds. This is managed by setting a "last ran"
    // timestamp, and running if it is more than `runInterval` seconds ago. Even with very short
    // intervals, the timer will only fire at most once every few minutes.
    const runInterval = prefs.getIntPref(RUN_INTERVAL_PREF);
    timerManager.registerTimer(TIMER_NAME, () => this.run(), runInterval);
  },

  async run() {
    this.clearCaches();
    // Unless lazy classification is enabled, prep the classify cache.
    if (!Preferences.get("extensions.shield-recipe-client.experiments.lazy_classify", false)) {
      try {
        await ClientEnvironment.getClientClassification();
      } catch (err) {
        // Try to go on without this data; the filter expressions will
        // gracefully fail without this info if they need it.
      }
    }

    // Fetch recipes before execution in case we fail and exit early.
    let recipes;
    try {
      recipes = await NormandyApi.fetchRecipes({enabled: true});
    } catch (e) {
      const apiUrl = prefs.getCharPref("api_url");
      log.error(`Could not fetch recipes from ${apiUrl}: "${e}"`);

      let status = Uptake.RUNNER_SERVER_ERROR;
      if (/NetworkError/.test(e)) {
        status = Uptake.RUNNER_NETWORK_ERROR;
      } else if (e instanceof NormandyApi.InvalidSignatureError) {
        status = Uptake.RUNNER_INVALID_SIGNATURE;
      }
      Uptake.reportRunner(status);
      return;
    }

    const actionSandboxManagers = await this.loadActionSandboxManagers();
    Object.values(actionSandboxManagers).forEach(manager => manager.addHold("recipeRunner"));

    // Run pre-execution hooks. If a hook fails, we don't run recipes with that
    // action to avoid inconsistencies.
    for (const [actionName, manager] of Object.entries(actionSandboxManagers)) {
      try {
        await manager.runAsyncCallback("preExecution");
        manager.disabled = false;
      } catch (err) {
        log.error(`Could not run pre-execution hook for ${actionName}:`, err.message);
        manager.disabled = true;
        Uptake.reportAction(actionName, Uptake.ACTION_PRE_EXECUTION_ERROR);
      }
    }

    // Evaluate recipe filters
    const recipesToRun = [];
    for (const recipe of recipes) {
      if (await this.checkFilter(recipe)) {
        recipesToRun.push(recipe);
      }
    }

    // Execute recipes, if we have any.
    if (recipesToRun.length === 0) {
      log.debug("No recipes to execute");
    } else {
      for (const recipe of recipesToRun) {
        const manager = actionSandboxManagers[recipe.action];
        let status;
        if (!manager) {
          log.error(
            `Could not execute recipe ${recipe.name}:`,
            `Action ${recipe.action} is either missing or invalid.`
          );
          status = Uptake.RECIPE_INVALID_ACTION;
        } else if (manager.disabled) {
          log.warn(
            `Skipping recipe ${recipe.name} because ${recipe.action} failed during pre-execution.`
          );
          status = Uptake.RECIPE_ACTION_DISABLED;
        } else {
          try {
            log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
            await manager.runAsyncCallback("action", recipe);
            status = Uptake.RECIPE_SUCCESS;
          } catch (e) {
            log.error(`Could not execute recipe ${recipe.name}:`);
            Cu.reportError(e);
            status = Uptake.RECIPE_EXECUTION_ERROR;
          }
        }

        Uptake.reportRecipe(recipe.id, status);
      }
    }

    // Run post-execution hooks
    for (const [actionName, manager] of Object.entries(actionSandboxManagers)) {
      // Skip if pre-execution failed.
      if (manager.disabled) {
        log.info(`Skipping post-execution hook for ${actionName} due to earlier failure.`);
        continue;
      }

      try {
        await manager.runAsyncCallback("postExecution");
        Uptake.reportAction(actionName, Uptake.ACTION_SUCCESS);
      } catch (err) {
        log.info(`Could not run post-execution hook for ${actionName}:`, err.message);
        Uptake.reportAction(actionName, Uptake.ACTION_POST_EXECUTION_ERROR);
      }
    }

    // Nuke sandboxes
    Object.values(actionSandboxManagers).forEach(manager => manager.removeHold("recipeRunner"));

    // Close storage connections
    await AddonStudies.close();

    Uptake.reportRunner(Uptake.RUNNER_SUCCESS);
  },

  async loadActionSandboxManagers() {
    const actions = await NormandyApi.fetchActions();
    const actionSandboxManagers = {};
    for (const action of actions) {
      try {
        const implementation = await NormandyApi.fetchImplementation(action);
        actionSandboxManagers[action.name] = new ActionSandboxManager(implementation);
      } catch (err) {
        log.warn(`Could not fetch implementation for ${action.name}:`, err);

        let status = Uptake.ACTION_SERVER_ERROR;
        if (/NetworkError/.test(err)) {
          status = Uptake.ACTION_NETWORK_ERROR;
        }
        Uptake.reportAction(action.name, status);
      }
    }
    return actionSandboxManagers;
  },

  getFilterContext(recipe) {
    return {
      normandy: Object.assign(ClientEnvironment.getEnvironment(), {
        recipe: {
          id: recipe.id,
          arguments: recipe.arguments,
        },
      }),
    };
  },

  /**
   * Evaluate a recipe's filter expression against the environment.
   * @param {object} recipe
   * @param {string} recipe.filter The expression to evaluate against the environment.
   * @return {boolean} The result of evaluating the filter, cast to a bool, or false
   *                   if an error occurred during evaluation.
   */
  async checkFilter(recipe) {
    const context = this.getFilterContext(recipe);
    try {
      const result = await FilterExpressions.eval(recipe.filter_expression, context);
      return !!result;
    } catch (err) {
      log.error(`Error checking filter for "${recipe.name}"`);
      log.error(`Filter: "${recipe.filter_expression}"`);
      log.error(`Error: "${err}"`);
      return false;
    }
  },

  /**
   * Clear all caches of systems used by RecipeRunner, in preparation
   * for a clean run.
   */
  clearCaches() {
    ClientEnvironment.clearClassifyCache();
    NormandyApi.clearIndexCache();
  },

  /**
   * Clear out cached state and fetch/execute recipes from the given
   * API url. This is used mainly by the mock-recipe-server JS that is
   * executed in the browser console.
   */
  async testRun(baseApiUrl) {
    const oldApiUrl = prefs.getCharPref("api_url");
    prefs.setCharPref("api_url", baseApiUrl);

    try {
      Storage.clearAllStorage();
      this.clearCaches();
      await this.run();
    } finally {
      prefs.setCharPref("api_url", oldApiUrl);
      this.clearCaches();
    }
  },
};
