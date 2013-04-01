/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 742944 - Test that window.open works with <iframe mozbrowser>.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  var gotPopup = false;
  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    is(gotPopup, false, 'Should get just one popup.');
    gotPopup = true;

    document.body.appendChild(e.detail.frameElement);

    ok(/file_browserElement_Open2\.html$/.test(e.detail.url),
       "Popup's URL (got " + e.detail.url + ")");
    is(e.detail.name, "name");
    is(e.detail.features, "dialog=1");
  });

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    ok(gotPopup, 'Got mozbrowseropenwindow event before showmodalprompt event.');
    if (e.detail.message.indexOf("success:") == 0) {
      ok(true, e.detail.message);
    }
    else if (e.detail.message.indexOf("failure:") == 0) {
      ok(false, e.detail.message);
    }
    else if (e.detail.message == "finish") {
      SimpleTest.finish();
    }
    else {
      ok(false, "Got invalid message: " + e.detail.message);
    }
  });

  /**
   * file_browserElementOpen1 does
   *
   *   window.open('file_browserElement_Open2.html', 'name', 'dialog=1')
   *
   * then adds an event listener to the opened window and waits for onload.
   *
   * Onload, we fire a few alerts saying "success:REASON" or "failure:REASON".
   * Finally, we fire a "finish" alert, which ends the test.
   */
  iframe.src = 'file_browserElement_Open1.html';
  document.body.appendChild(iframe);
}

addEventListener('testready', runTest);
