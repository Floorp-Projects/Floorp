/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function runTest()
{
  SimpleTest.waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({'set': [ ["dom.archivereader.enabled", true] ]}, function() {
    return testGenerator.next();
  });
}

function finishTest()
{
  SpecialPowers.popPrefEnv(function() {
    testGenerator.close();
    SimpleTest.finish();
  });
}
