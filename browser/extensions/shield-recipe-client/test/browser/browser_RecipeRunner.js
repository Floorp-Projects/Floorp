"use strict";

Cu.import("resource://testing-common/TestUtils.jsm", this);
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);
Cu.import("resource://shield-recipe-client/lib/ClientEnvironment.jsm", this);
Cu.import("resource://shield-recipe-client/lib/CleanupManager.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyApi.jsm", this);
Cu.import("resource://shield-recipe-client/lib/ActionSandboxManager.jsm", this);
Cu.import("resource://shield-recipe-client/lib/AddonStudies.jsm", this);
Cu.import("resource://shield-recipe-client/lib/Uptake.jsm", this);

add_task(async function getFilterContext() {
  const recipe = {id: 17, arguments: {foo: "bar"}, unrelated: false};
  const context = RecipeRunner.getFilterContext(recipe);

  // Test for expected properties in the filter expression context.
  const expectedNormandyKeys = [
    "channel",
    "country",
    "distribution",
    "doNotTrack",
    "isDefaultBrowser",
    "locale",
    "plugins",
    "recipe",
    "request_time",
    "searchEngine",
    "syncDesktopDevices",
    "syncMobileDevices",
    "syncSetup",
    "syncTotalDevices",
    "telemetry",
    "userId",
    "version",
  ];
  for (const key of expectedNormandyKeys) {
    ok(key in context.normandy, `normandy.${key} is available`);
  }

  is(
    context.normandy.recipe.id,
    recipe.id,
    "normandy.recipe is the recipe passed to getFilterContext",
  );
  delete recipe.unrelated;
  Assert.deepEqual(
    context.normandy.recipe,
    recipe,
    "normandy.recipe drops unrecognized attributes from the recipe",
  );
});

add_task(async function checkFilter() {
  const check = filter => RecipeRunner.checkFilter({filter_expression: filter});

  // Errors must result in a false return value.
  ok(!(await check("invalid ( + 5yntax")), "Invalid filter expressions return false");

  // Non-boolean filter results result in a true return value.
  ok(await check("[1, 2, 3]"), "Non-boolean filter expressions return true");

  // The given recipe must be available to the filter context.
  const recipe = {filter_expression: "normandy.recipe.id == 7", id: 7};
  ok(await RecipeRunner.checkFilter(recipe), "The recipe is available in the filter context");
  recipe.id = 4;
  ok(!(await RecipeRunner.checkFilter(recipe)), "The recipe is available in the filter context");
});

add_task(withMockNormandyApi(async function testClientClassificationCache() {
  const getStub = sinon.stub(ClientEnvironment, "getClientClassification")
    .returns(Promise.resolve(false));

  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.shield-recipe-client.api_url",
      "https://example.com/selfsupport-dummy"],
  ]});

  // When the experiment pref is false, eagerly call getClientClassification.
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.shield-recipe-client.experiments.lazy_classify", false],
  ]});
  ok(!getStub.called, "getClientClassification hasn't been called");
  await RecipeRunner.run();
  ok(getStub.called, "getClientClassification was called eagerly");

  // When the experiment pref is true, do not eagerly call getClientClassification.
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.shield-recipe-client.experiments.lazy_classify", true],
  ]});
  getStub.reset();
  ok(!getStub.called, "getClientClassification hasn't been called");
  await RecipeRunner.run();
  ok(!getStub.called, "getClientClassification was not called eagerly");

  getStub.restore();
}));

/**
 * Mocks RecipeRunner.loadActionSandboxManagers for testing run.
 */
async function withMockActionSandboxManagers(actions, testFunction) {
  const managers = {};
  for (const action of actions) {
    const manager = new ActionSandboxManager("");
    manager.addHold("testing");
    managers[action.name] = manager;
    sinon.stub(managers[action.name], "runAsyncCallback");
  }

  const loadActionSandboxManagers = sinon.stub(RecipeRunner, "loadActionSandboxManagers")
    .resolves(managers);
  await testFunction(managers);
  loadActionSandboxManagers.restore();

  for (const manager of Object.values(managers)) {
    manager.removeHold("testing");
    await manager.isNuked();
  }
}

add_task(withMockNormandyApi(async function testRun(mockApi) {
  const closeSpy = sinon.spy(AddonStudies, "close");
  const reportRunner = sinon.stub(Uptake, "reportRunner");
  const reportAction = sinon.stub(Uptake, "reportAction");
  const reportRecipe = sinon.stub(Uptake, "reportRecipe");

  const matchAction = {name: "matchAction"};
  const noMatchAction = {name: "noMatchAction"};
  mockApi.actions = [matchAction, noMatchAction];

  const matchRecipe = {id: "match", action: "matchAction", filter_expression: "true"};
  const noMatchRecipe = {id: "noMatch", action: "noMatchAction", filter_expression: "false"};
  const missingRecipe = {id: "missing", action: "missingAction", filter_expression: "true"};
  mockApi.recipes = [matchRecipe, noMatchRecipe, missingRecipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const matchManager = managers.matchAction;
    const noMatchManager = managers.noMatchAction;

    await RecipeRunner.run();

    // match should be called for preExecution, action, and postExecution
    sinon.assert.calledWith(matchManager.runAsyncCallback, "preExecution");
    sinon.assert.calledWith(matchManager.runAsyncCallback, "action", matchRecipe);
    sinon.assert.calledWith(matchManager.runAsyncCallback, "postExecution");

    // noMatch should be called for preExecution and postExecution, and skipped
    // for action since the filter expression does not match.
    sinon.assert.calledWith(noMatchManager.runAsyncCallback, "preExecution");
    sinon.assert.neverCalledWith(noMatchManager.runAsyncCallback, "action", noMatchRecipe);
    sinon.assert.calledWith(noMatchManager.runAsyncCallback, "postExecution");

    // missing is never called at all due to no matching action/manager.

    // Test uptake reporting
    sinon.assert.calledWith(reportRunner, Uptake.RUNNER_SUCCESS);
    sinon.assert.calledWith(reportAction, "matchAction", Uptake.ACTION_SUCCESS);
    sinon.assert.calledWith(reportAction, "noMatchAction", Uptake.ACTION_SUCCESS);
    sinon.assert.calledWith(reportRecipe, "match", Uptake.RECIPE_SUCCESS);
    sinon.assert.neverCalledWith(reportRecipe, "noMatch", Uptake.RECIPE_SUCCESS);
    sinon.assert.calledWith(reportRecipe, "missing", Uptake.RECIPE_INVALID_ACTION);
  });

  // Ensure storage is closed after the run.
  sinon.assert.calledOnce(closeSpy);

  closeSpy.restore();
  reportRunner.restore();
  reportAction.restore();
  reportRecipe.restore();
}));

add_task(withMockNormandyApi(async function testRunRecipeError(mockApi) {
  const reportRecipe = sinon.stub(Uptake, "reportRecipe");

  const action = {name: "action"};
  mockApi.actions = [action];

  const recipe = {id: "recipe", action: "action", filter_expression: "true"};
  mockApi.recipes = [recipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const manager = managers.action;
    manager.runAsyncCallback.callsFake(async callbackName => {
      if (callbackName === "action") {
        throw new Error("Action execution failure");
      }
    });

    await RecipeRunner.run();

    // Uptake should report that the recipe threw an exception
    sinon.assert.calledWith(reportRecipe, "recipe", Uptake.RECIPE_EXECUTION_ERROR);
  });

  reportRecipe.restore();
}));

add_task(withMockNormandyApi(async function testRunFetchFail(mockApi) {
  const closeSpy = sinon.spy(AddonStudies, "close");
  const reportRunner = sinon.stub(Uptake, "reportRunner");

  const action = {name: "action"};
  mockApi.actions = [action];
  mockApi.fetchRecipes.rejects(new Error("Signature not valid"));

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const manager = managers.action;
    await RecipeRunner.run();

    // If the recipe fetch failed, do not run anything.
    sinon.assert.notCalled(manager.runAsyncCallback);
    sinon.assert.calledWith(reportRunner, Uptake.RUNNER_SERVER_ERROR);

    // Test that network errors report a specific uptake error
    reportRunner.reset();
    mockApi.fetchRecipes.rejects(new Error("NetworkError: The system was down"));
    await RecipeRunner.run();
    sinon.assert.calledWith(reportRunner, Uptake.RUNNER_NETWORK_ERROR);

    // Test that signature issues report a specific uptake error
    reportRunner.reset();
    mockApi.fetchRecipes.rejects(new NormandyApi.InvalidSignatureError("Signature fail"));
    await RecipeRunner.run();
    sinon.assert.calledWith(reportRunner, Uptake.RUNNER_INVALID_SIGNATURE);
  });

  // If the recipe fetch failed, we don't need to call close since nothing
  // opened a connection in the first place.
  sinon.assert.notCalled(closeSpy);

  closeSpy.restore();
  reportRunner.restore();
}));

add_task(withMockNormandyApi(async function testRunPreExecutionFailure(mockApi) {
  const closeSpy = sinon.spy(AddonStudies, "close");
  const reportAction = sinon.stub(Uptake, "reportAction");
  const reportRecipe = sinon.stub(Uptake, "reportRecipe");

  const passAction = {name: "passAction"};
  const failAction = {name: "failAction"};
  mockApi.actions = [passAction, failAction];

  const passRecipe = {id: "pass", action: "passAction", filter_expression: "true"};
  const failRecipe = {id: "fail", action: "failAction", filter_expression: "true"};
  mockApi.recipes = [passRecipe, failRecipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const passManager = managers.passAction;
    const failManager = managers.failAction;
    failManager.runAsyncCallback.returns(Promise.reject(new Error("oh no")));

    await RecipeRunner.run();

    // pass should be called for preExecution, action, and postExecution
    sinon.assert.calledWith(passManager.runAsyncCallback, "preExecution");
    sinon.assert.calledWith(passManager.runAsyncCallback, "action", passRecipe);
    sinon.assert.calledWith(passManager.runAsyncCallback, "postExecution");

    // fail should only be called for preExecution, since it fails during that
    sinon.assert.calledWith(failManager.runAsyncCallback, "preExecution");
    sinon.assert.neverCalledWith(failManager.runAsyncCallback, "action", failRecipe);
    sinon.assert.neverCalledWith(failManager.runAsyncCallback, "postExecution");

    sinon.assert.calledWith(reportAction, "passAction", Uptake.ACTION_SUCCESS);
    sinon.assert.calledWith(reportAction, "failAction", Uptake.ACTION_PRE_EXECUTION_ERROR);
    sinon.assert.calledWith(reportRecipe, "fail", Uptake.RECIPE_ACTION_DISABLED);
  });

  // Ensure storage is closed after the run, despite the failures.
  sinon.assert.calledOnce(closeSpy);
  closeSpy.restore();
  reportAction.restore();
  reportRecipe.restore();
}));

add_task(withMockNormandyApi(async function testRunPostExecutionFailure(mockApi) {
  const reportAction = sinon.stub(Uptake, "reportAction");

  const failAction = {name: "failAction"};
  mockApi.actions = [failAction];

  const failRecipe = {action: "failAction", filter_expression: "true"};
  mockApi.recipes = [failRecipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const failManager = managers.failAction;
    failManager.runAsyncCallback.callsFake(async callbackName => {
      if (callbackName === "postExecution") {
        throw new Error("postExecution failure");
      }
    });

    await RecipeRunner.run();

    // fail should be called for every stage
    sinon.assert.calledWith(failManager.runAsyncCallback, "preExecution");
    sinon.assert.calledWith(failManager.runAsyncCallback, "action", failRecipe);
    sinon.assert.calledWith(failManager.runAsyncCallback, "postExecution");

    // Uptake should report a post-execution error
    sinon.assert.calledWith(reportAction, "failAction", Uptake.ACTION_POST_EXECUTION_ERROR);
  });

  reportAction.restore();
}));

add_task(withMockNormandyApi(async function testLoadActionSandboxManagers(mockApi) {
  mockApi.actions = [
    {name: "normalAction"},
    {name: "missingImpl"},
  ];
  mockApi.implementations.normalAction = "window.scriptRan = true";

  const managers = await RecipeRunner.loadActionSandboxManagers();
  ok("normalAction" in managers, "Actions with implementations have managers");
  ok(!("missingImpl" in managers), "Actions without implementations are skipped");

  const normalManager = managers.normalAction;
  ok(
    await normalManager.evalInSandbox("window.scriptRan"),
    "Implementations are run in the sandbox",
  );
}));

// test init() in dev mode
decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.dev_mode", true],
      ["extensions.shield-recipe-client.first_run", false],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  async function testInitDevMode(runStub, registerTimerStub, updateRunIntervalStub) {
    await RecipeRunner.init();
    ok(runStub.called, "RecipeRunner.run is called immediately when in dev mode");
    ok(registerTimerStub.called, "RecipeRunner.init registers a timer");
  }
);

// Test init() during normal operation
decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.dev_mode", false],
      ["extensions.shield-recipe-client.first_run", false],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  async function testInit(runStub, registerTimerStub) {
    await RecipeRunner.init();
    ok(!runStub.called, "RecipeRunner.run is called immediately when not in dev mode or first run");
    ok(registerTimerStub.called, "RecipeRunner.init registers a timer");
  }
);

// Test init() first run
decorate_task(
  withPrefEnv({
    set: [
      ["extensions.shield-recipe-client.dev_mode", false],
      ["extensions.shield-recipe-client.first_run", true],
      ["extensions.shield-recipe-client.api_url", "https://example.com"],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  withStub(RecipeRunner, "watchPrefs"),
  async function testInitFirstRun(runStub, registerTimerStub, watchPrefsStub) {
    await RecipeRunner.init();
    ok(runStub.called, "RecipeRunner.run is called immediately on first run");
    ok(
      !Services.prefs.getBoolPref("extensions.shield-recipe-client.first_run"),
      "On first run, the first run pref is set to false"
    );
    ok(registerTimerStub.called, "RecipeRunner.registerTimer registers a timer");

    // RecipeRunner.init() sets this to false, but SpecialPowers
    // relies on the preferences it manages to actually change when it
    // tries to change them. Settings this back to true here allows
    // that to happen. Not doing this causes popPrefEnv to hang forever.
    Services.prefs.setBoolPref("extensions.shield-recipe-client.first_run", true);
  }
);

// Test that prefs are watched correctly
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],  // telemetry enabled
      ["extensions.shield-recipe-client.dev_mode", false],
      ["extensions.shield-recipe-client.first_run", false],
      ["extensions.shield-recipe-client.enabled", true],
      ["extensions.shield-recipe-client.api_url", "https://example.com"], // starts with "https://"
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "enable"),
  withStub(RecipeRunner, "disable"),
  withStub(CleanupManager, "addCleanupHandler"),

  async function testPrefWatching(runStub, enableStub, disableStub, addCleanupHandlerStub) {
    await RecipeRunner.init();
    is(enableStub.callCount, 1, "Enable should be called initially");
    is(disableStub.callCount, 0, "Disable should not be called initially");

    await SpecialPowers.pushPrefEnv({ set: [["extensions.shield-recipe-client.enabled", false]] });
    is(enableStub.callCount, 1, "Enable should not be called again");
    is(disableStub.callCount, 1, "RecipeRunner should disable when Shield is disabled");

    await SpecialPowers.pushPrefEnv({ set: [["extensions.shield-recipe-client.enabled", true]] });
    is(enableStub.callCount, 2, "RecipeRunner should re-enable when Shield is enabled");
    is(disableStub.callCount, 1, "Disable should not be called again");

    await SpecialPowers.pushPrefEnv({ set: [["extensions.shield-recipe-client.api_url", "http://example.com"]] }); // does not start with https://
    is(enableStub.callCount, 2, "Enable should not be called again");
    is(disableStub.callCount, 2, "RecipeRunner should disable when an invalid api url is given");

    await SpecialPowers.pushPrefEnv({ set: [["extensions.shield-recipe-client.api_url", "https://example.com"]] }); // ends with https://
    is(enableStub.callCount, 3, "RecipeRunner should re-enable when a valid api url is given");
    is(disableStub.callCount, 2, "Disable should not be called again");

    await SpecialPowers.pushPrefEnv({ set: [["datareporting.healthreport.uploadEnabled", false]] });
    is(enableStub.callCount, 3, "Enable should not be called again");
    is(disableStub.callCount, 3, "RecipeRunner should disable when telemetry is disabled");

    await SpecialPowers.pushPrefEnv({ set: [["datareporting.healthreport.uploadEnabled", true]] });
    is(enableStub.callCount, 4, "RecipeRunner should re-enable when telemetry is enabled");
    is(disableStub.callCount, 3, "Disable should not be called again");

    is(runStub.callCount, 0, "RecipeRunner.run should not be called during this test");
  }
);

// Test that enable and disable are idempotent
decorate_task(
  withStub(RecipeRunner, "registerTimer"),
  withStub(RecipeRunner, "unregisterTimer"),

  async function testPrefWatching(registerTimerStub, unregisterTimerStub) {
    const originalEnabled = RecipeRunner.enabled;

    try {
      RecipeRunner.enabled = false;
      RecipeRunner.enable();
      RecipeRunner.enable();
      RecipeRunner.enable();
      is(registerTimerStub.callCount, 1, "Enable should be idempotent");

      RecipeRunner.enabled = true;
      RecipeRunner.disable();
      RecipeRunner.disable();
      RecipeRunner.disable();
      is(registerTimerStub.callCount, 1, "Disable should be idempotent");

    } finally {
      RecipeRunner.enabled = originalEnabled;
    }
  }
);
