/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu } = Components;

let testGenerator = testSteps();

function runTest()
{
  SimpleTest.waitForExplicitFinish();

  testGenerator.next();
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
