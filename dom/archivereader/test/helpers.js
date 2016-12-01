/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function runTest()
{
  SimpleTest.waitForExplicitFinish();

  SpecialPowers.pushPrefEnv({'set': [ ["dom.archivereader.enabled", true] ]}, function() {
    SpecialPowers.createFiles(filesToCreate(),
                              function (files) {
                                testSteps(files);
                              },
                              function (msg) {
                                ok(false, "File creation error: " + msg);
                                finishTest();
                              });
  });
}

function finishTest()
{
  SpecialPowers.popPrefEnv(function() {
    SimpleTest.finish();
  });
}
