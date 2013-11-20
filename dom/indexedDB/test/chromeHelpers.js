/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu } = Components;

let testGenerator = testSteps();

if (!window.runTest) {
  window.runTest = function()
  {
    Cu.importGlobalProperties(["indexedDB"]);

    SimpleTest.waitForExplicitFinish();

    testGenerator.next();
  }
}

function finishTest()
{
  SimpleTest.executeSoon(function() {
    testGenerator.close();
    SimpleTest.finish();
  });
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function continueToNextStep()
{
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function errorHandler(event)
{
  throw new Error("indexedDB error, code " + event.target.error.name);
}
