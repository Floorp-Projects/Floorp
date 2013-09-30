/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  // Test for IDBKeyRange and indexedDB availability in ipcshell.
  run_test_in_child("./GlobalObjectsChild.js", function() {
    do_test_finished();
    continueToNextStep();
  });
  yield undefined;

  finishTest();
  yield undefined;
}
