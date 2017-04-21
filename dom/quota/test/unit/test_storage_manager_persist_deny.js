var disableWorkerTest = "Persist doesn't work in workers";
var testGenerator = testSteps();

function* testSteps()
{
  SpecialPowers.pushPrefEnv({
    "set": [["dom.storageManager.prompt.testing.allow", false]]
  }, continueToNextStep);
  yield undefined;

  navigator.storage.persist().then(grabArgAndContinueHandler);
  let persistResult = yield undefined;

  is(persistResult, false, "Cancel the persist prompt and resolve a promise with false");

  navigator.storage.persisted().then(grabArgAndContinueHandler);
  let persistedResult = yield undefined;

  is(persistResult, persistedResult, "Persist/persisted results are consistent");

  finishTest();
}
