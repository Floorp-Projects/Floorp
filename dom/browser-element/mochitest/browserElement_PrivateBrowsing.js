/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the mozprivatebrowsing attribute works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function createFrame(aIsPrivate) {
  var iframe = document.createElement("iframe");
  SpecialPowers.wrap(iframe).mozbrowser = true;
  if (aIsPrivate) {
    iframe.setAttribute("mozprivatebrowsing", "true");
  }
  return iframe;
}

function createTest(aIsPrivate, aExpected, aNext) {
  info("createTest " + aIsPrivate + " " + aExpected);
  var deferred = Promise.defer();

  var iframe = createFrame(aIsPrivate);
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    is(e.detail.message, aExpected, "Checking localstorage");
    deferred.resolve();
  });

  iframe.src = "file_browserElement_PrivateBrowsing.html";

  return deferred.promise;
}

function runTest() {
  // We first create a iframe in non private browsing mode, set up some
  // localstorage, reopen it to check that we get the previously set value.
  // Finally, open it in private browsing mode and check that localstorage
  // is clear.
  createTest(false, "EMPTY")
  .then(() => { return createTest(false, "bar"); })
  .then(() => { return createTest(true, "EMPTY"); })
  .then(SimpleTest.finish);
}

addEventListener("testready", runTest);
