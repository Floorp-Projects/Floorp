/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const name = "test_snappyUpgrade.js";
  const objectStoreName = "test";
  const testString = "Lorem ipsum his ponderum delicatissimi ne, at noster dolores urbanitas pro, cibo elaboraret no his. Ea dicunt maiorum usu. Ad appareat facilisis mediocritatem eos. Tale graeci mentitum in eos, hinc insolens at nam. Graecis nominavi aliquyam eu vix. Id solet assentior sadipscing pro. Et per atqui graecis, usu quot viris repudiandae ei, mollis evertitur an nam. At nam dolor ignota, liber labore omnesque ea mei, has movet voluptaria in. Vel an impetus omittantur. Vim movet option salutandi ex, ne mei ignota corrumpit. Mucius comprehensam id per. Est ea putant maiestatis.";

  info("Installing profile");

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("snappyUpgrade_profile");

  info("Opening database");

  let request = indexedDB.open(name);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  // success
  let db = request.result;
  db.onerror = errorHandler;

  info("Getting string");

  request = db.transaction([objectStoreName])
              .objectStore(objectStoreName).get(1);
  request.onsuccess = continueToNextStepSync;
  yield undefined;

  is(request.result, testString);

  finishTest();
  yield undefined;
}
