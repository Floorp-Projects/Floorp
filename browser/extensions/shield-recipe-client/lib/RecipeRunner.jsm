/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm"); /* globals setTimeout */
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm");
Cu.import("resource://shield-recipe-client/lib/EnvExpressions.jsm");
Cu.import("resource://shield-recipe-client/lib/NormandyApi.jsm");
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm");
Cu.importGlobalProperties(["fetch"]); /* globals fetch */

this.EXPORTED_SYMBOLS = ["RecipeRunner"];

const log = Log.repository.getLogger("extensions.shield-recipe-client");
const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");

this.RecipeRunner = {
  init() {
    if (!this.checkPrefs()) {
      return;
    }

    let delay;
    if (prefs.getBoolPref("dev_mode")) {
      delay = 0;
    } else {
      // startup delay is in seconds
      delay = prefs.getIntPref("startup_delay_seconds") * 1000;
    }

    setTimeout(this.start.bind(this), delay);
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

  start: Task.async(function* () {
    let recipes;
    try {
      recipes = yield NormandyApi.fetchRecipes({enabled: true});
    } catch (e) {
      const apiUrl = prefs.getCharPref("api_url");
      log.error(`Could not fetch recipes from ${apiUrl}: "${e}"`);
      return;
    }

    let extraContext;
    try {
      extraContext = yield this.getExtraContext();
    } catch (e) {
      log.warning(`Couldn't get extra filter context: ${e}`);
      extraContext = {};
    }

    const recipesToRun = [];

    for (const recipe of recipes) {
      if (yield this.checkFilter(recipe, extraContext)) {
        recipesToRun.push(recipe);
      }
    }

    if (recipesToRun.length === 0) {
      log.debug("No recipes to execute");
    } else {
      for (const recipe of recipesToRun) {
        try {
          log.debug(`Executing recipe "${recipe.name}" (action=${recipe.action})`);
          yield this.executeRecipe(recipe, extraContext);
        } catch (e) {
          log.error(`Could not execute recipe ${recipe.name}:`, e);
        }
      }
    }
  }),

  getExtraContext() {
    return NormandyApi.classifyClient()
      .then(clientData => ({normandy: clientData}));
  },

  /**
   * Evaluate a recipe's filter expression against the environment.
   * @param {object} recipe
   * @param {string} recipe.filter The expression to evaluate against the environment.
   * @param {object} extraContext Any extra context to provide to the filter environment.
   * @return {boolean} The result of evaluating the filter, cast to a bool.
   */
  checkFilter(recipe, extraContext) {
    return EnvExpressions.eval(recipe.filter_expression, extraContext)
      .then(result => {
        return !!result;
      })
      .catch(error => {
        log.error(`Error checking filter for "${recipe.name}"`);
        log.error(`Filter: "${recipe.filter_expression}"`);
        log.error(`Error: "${error}"`);
      });
  },

  /**
   * Execute a recipe by fetching it action and executing it.
   * @param  {Object} recipe A recipe to execute
   * @promise Resolves when the action has executed
   */
  executeRecipe: Task.async(function* (recipe, extraContext) {
    const sandboxManager = new SandboxManager();
    const {sandbox} = sandboxManager;

    const action = yield NormandyApi.fetchAction(recipe.action);
    const response = yield fetch(action.implementation_url);

    const actionScript = yield response.text();
    const prepScript = `
      var pendingAction = null;

      function registerAction(name, Action) {
        let a = new Action(sandboxedDriver, sandboxedRecipe);
        pendingAction = a.execute()
          .catch(err => sandboxedDriver.log(err, 'error'));
      };

      window.registerAction = registerAction;
      window.setTimeout = sandboxedDriver.setTimeout;
      window.clearTimeout = sandboxedDriver.clearTimeout;
    `;

    const driver = new NormandyDriver(sandboxManager, extraContext);
    sandbox.sandboxedDriver = Cu.cloneInto(driver, sandbox, {cloneFunctions: true});
    sandbox.sandboxedRecipe = Cu.cloneInto(recipe, sandbox);

    Cu.evalInSandbox(prepScript, sandbox);
    Cu.evalInSandbox(actionScript, sandbox);

    sandboxManager.addHold("recipeExecution");
    sandbox.pendingAction.then(() => sandboxManager.removeHold("recipeExecution"));
  }),
};
