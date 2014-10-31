/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 766871 - Test that window.open works from an <iframe> within <iframe mozbrowser>.
//
// This is basically the same as browserElement_OpenWindow, except that instead
// of loading file_browserElement_Open1.html directly inside the <iframe
// mozbrowser>, we load file_browserElement_OpenWindowInFrame.html into the
// mozbrowser.  OpenWindowInFrame loads file_browserElement_Open1.html inside
// an iframe.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

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
   * file_browserElement_OpenWindowInFrame.html loads
   * file_browserElement_Open1.html in an iframe.  Open1.html does
   *
   *   window.open('file_browserElement_Open2.html', 'name', 'dialog=1')
   *
   * then adds an event listener to the opened window and waits for onload.
   *
   * Onload, we fire a few alerts saying "success:REASON" or "failure:REASON".
   * Finally, we fire a "finish" alert, which ends the test.
   */
  iframe.src = 'file_browserElement_OpenWindowInFrame.html';
  document.body.appendChild(iframe);
}

addEventListener('testready', runTest);
