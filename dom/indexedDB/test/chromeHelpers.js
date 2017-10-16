/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var { "classes": Cc, "interfaces": Ci, "utils": Cu } = Components;

// testSteps is expected to be defined by the file including this file.
/* global testSteps */

var testGenerator = testSteps();

if (!window.runTest) {
  window.runTest = function()
  {
    SimpleTest.waitForExplicitFinish();

    testGenerator.next();
  };
}

function finishTest()
{
  SimpleTest.executeSoon(function() {
    testGenerator.return();
    SimpleTest.finish();
  });
}

function grabEventAndContinueHandler(event)
{
  testGenerator.next(event);
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
