/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function* testSteps() {
  navigator.storage.persisted().then(grabArgAndContinueHandler);
  let persistedResult = yield undefined;

  is(persistedResult, false, "Persisted returns false");

  finishTest();
}
