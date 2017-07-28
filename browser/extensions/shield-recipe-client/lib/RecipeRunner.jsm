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

Cu.importGlobalProperties(["fetch"]);

this.EXPORTED_SYMBOLS = ["RecipeRunner"];

const log = LogManager.getLogger("recipe-runner");
const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");
const TIMER_NAME = "recipe-client-addon-run";
const RUN_INTERVAL_PREF = "run_interval_seconds";
const FIRST_RUN_PREF = "first_run";
const UI_AVAILABLE_NOTIFICATION = "sessionstore-windows-restored";
const SHIELD_INIT_NOTIFICATION = "shield-init-complete";

this.RecipeRunner = {
  init() {
    if (!this.checkPrefs()) {
      return;
    }

    if (prefs.getBoolPref("dev_mode")) {
      // Run right now in dev mode
      this.run();
    }

    if (prefs.getBoolPref(FIRST_RUN_PREF)) {
      // Run once immediately after the UI is available. Do this before adding the
      // timer so we can't end up racing it.
      const observer = {
        observe: async (subject, topic, data) => {
          Services.obs.removeObserver(observer, UI_AVAILABLE_NOTIFICATION);

          await this.run();
          this.registerTimer();
          prefs.setBoolPref(FIRST_RUN_PREF, false);

          Services.obs.notifyObservers(null, SHIELD_INIT_NOTIFICATION);
        },
      };
      Services.obs.addObserver(observer, UI_AVAILABLE_NOTIFICATION);
      CleanupManager.addCleanupHandler(() => Services.obs.removeObserver(observer, UI_AVAILABLE_NOTIFICATION));
    } else {
      this.registerTimer();
    }
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

  /**
   * Watch for preference changes from Services.pref.addObserver.
   */
  observe(changedPrefBranch, action, changedPref) {
    if (action === "nsPref:changed" && changedPref === RUN_INTERVAL_PREF) {
      this.updateRunInterval();
    } else {
      log.debug(`Observer fired with unexpected pref change: ${action} ${changedPref}`);
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
      await ClientEnvironment.getClientClassification();
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
      }
    }

    // Fetch recipes from the API
    let recipes;
    try {
      recipes = await NormandyApi.fetchRecipes({enabled: true});
    } catch (e) {
      const apiUrl = prefs.getCharPref("api_url");
      log.error(`Could not fetch recipes from ${apiUrl}: "${e}"`);
      return;
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
        if (!manager) {
          log.error(
            `Could not execute recipe ${recipe.name}:`,
            `Action ${recipe.action} is either missing or invalid.`
          );
        } else if (manager.disabled) {
          log.warn(
            `Skipping recipe ${recipe.name} because ${recipe.action} failed during pre-execution.`
          );
        } else {
          try {
            log.info(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
            await manager.runAsyncCallback("action", recipe);
          } catch (e) {
            log.error(`Could not execute recipe ${recipe.name}:`, e);
          }
        }
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
      } catch (err) {
        log.info(`Could not run post-execution hook for ${actionName}:`, err.message);
      }
    }

    // Nuke sandboxes
    Object.values(actionSandboxManagers).forEach(manager => manager.removeHold("recipeRunner"));
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
