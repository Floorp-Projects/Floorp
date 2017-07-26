"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);
Cu.import("resource://shield-recipe-client/lib/ClientEnvironment.jsm", this);
Cu.import("resource://shield-recipe-client/lib/CleanupManager.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyApi.jsm", this);
Cu.import("resource://shield-recipe-client/lib/ActionSandboxManager.jsm", this);

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
    managers[action.name] = new ActionSandboxManager("");
    sinon.stub(managers[action.name], "runAsyncCallback");
  }

  const loadActionSandboxManagers = sinon.stub(
    RecipeRunner,
    "loadActionSandboxManagers",
    async () => managers,
  );
  await testFunction(managers);
  loadActionSandboxManagers.restore();
}

add_task(withMockNormandyApi(async function testRun(mockApi) {
  const matchAction = {name: "matchAction"};
  const noMatchAction = {name: "noMatchAction"};
  mockApi.actions = [matchAction, noMatchAction];

  const matchRecipe = {action: "matchAction", filter_expression: "true"};
  const noMatchRecipe = {action: "noMatchAction", filter_expression: "false"};
  const missingRecipe = {action: "missingAction", filter_expression: "true"};
  mockApi.recipes = [matchRecipe, noMatchRecipe, missingRecipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const matchManager = managers["matchAction"];
    const noMatchManager = managers["noMatchAction"];

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
    await matchManager.isNuked();
    await noMatchManager.isNuked();
  });
}));

add_task(withMockNormandyApi(async function testRunPreExecutionFailure(mockApi) {
  const passAction = {name: "passAction"};
  const failAction = {name: "failAction"};
  mockApi.actions = [passAction, failAction];

  const passRecipe = {action: "passAction", filter_expression: "true"};
  const failRecipe = {action: "failAction", filter_expression: "true"};
  mockApi.recipes = [passRecipe, failRecipe];

  await withMockActionSandboxManagers(mockApi.actions, async managers => {
    const passManager = managers["passAction"];
    const failManager = managers["failAction"];
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

    await passManager.isNuked();
    await failManager.isNuked();
  });
}));

add_task(withMockNormandyApi(async function testLoadActionSandboxManagers(mockApi) {
  mockApi.actions = [
    {name: "normalAction"},
    {name: "missingImpl"},
  ];
  mockApi.implementations["normalAction"] = "window.scriptRan = true";

  const managers = await RecipeRunner.loadActionSandboxManagers();
  ok("normalAction" in managers, "Actions with implementations have managers");
  ok(!("missingImpl" in managers), "Actions without implementations are skipped");

  const normalManager = managers["normalAction"];
  ok(
    await normalManager.evalInSandbox("window.scriptRan"),
    "Implementations are run in the sandbox",
  );
}));

add_task(async function testStartup() {
  const runStub = sinon.stub(RecipeRunner, "run");
  const addCleanupHandlerStub = sinon.stub(CleanupManager, "addCleanupHandler");
  const updateRunIntervalStub = sinon.stub(RecipeRunner, "updateRunInterval");

  // in dev mode
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.shield-recipe-client.dev_mode", true],
      ["extensions.shield-recipe-client.first_run", false],
    ],
  });

  RecipeRunner.init();
  ok(runStub.called, "RecipeRunner.run is called immediately when in dev mode");
  ok(addCleanupHandlerStub.called, "A cleanup function is registered when in dev mode");
  ok(updateRunIntervalStub.called, "A timer is registered when in dev mode");

  runStub.reset();
  addCleanupHandlerStub.reset();
  updateRunIntervalStub.reset();

  // not in dev mode
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.shield-recipe-client.dev_mode", false],
      ["extensions.shield-recipe-client.first_run", false],
    ],
  });

  RecipeRunner.init();
  ok(!runStub.called, "RecipeRunner.run is not called immediately when not in dev mode");
  ok(addCleanupHandlerStub.called, "A cleanup function is registered when not in dev mode");
  ok(updateRunIntervalStub.called, "A timer is registered when not in dev mode");

  runStub.restore();
  addCleanupHandlerStub.restore();
  updateRunIntervalStub.restore();
});
