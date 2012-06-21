/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that <iframe mozbrowser>'s have a different principal than
// their creator.
"use strict";

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  document.body.appendChild(iframe);

  if (!iframe.contentWindow) {
    ok(true, "OOP, can't access contentWindow.");
    return;
  }

  SimpleTest.waitForExplicitFinish();

  // Try reading iframe.contentWindow.location now, and then from a timeout.
  // They both should throw exceptions.
  checkCantReadLocation(iframe);
  SimpleTest.executeSoon(function() {
    checkCantReadLocation(iframe);
    SimpleTest.finish();
  });
}

function checkCantReadLocation(iframe) {
  try {
    if (iframe.contentWindow.location == 'foo') {
      ok(false, 'not reached');
    }
    ok(false, 'should have gotten exception');
  }
  catch(e) {
    ok(true, 'got exception reading contentWindow.location');
  }
}

runTest();
