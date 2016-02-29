/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

var testResult;
var testException;

function runTest()
{
  testGenerator.next();
}

function finishTestNow()
{
  if (testGenerator) {
    testGenerator.close();
    testGenerator = undefined;
  }
}

function finishTest()
{
  setTimeout(finishTestNow, 0);
  setTimeout(() => {
    if (window.testFinishedCallback)
      window.testFinishedCallback(testResult, testException);
    else {
      let message;
      if (testResult)
        message = "ok";
      else
        message = testException;
      window.parent.postMessage(message, "*");
    }
  }, 0);
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function errorHandler(event)
{
  throw new Error("indexedDB error, code " + event.target.error.name);
}

function continueToNextStep()
{
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}
