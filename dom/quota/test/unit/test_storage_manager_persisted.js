var testGenerator = testSteps();

function* testSteps() {
  navigator.storage.persisted().then(grabArgAndContinueHandler);
  let persistedResult = yield undefined;

  is(persistedResult, false, "Persisted returns false");

  finishTest();
}
