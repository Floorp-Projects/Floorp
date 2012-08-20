/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 776129 - If a window w calls window.open, the resultant window should be
// remote iff w is remote.
//
// <iframe mozbrowser> can be default-OOP or default-in-process.  But we can
// override this default by setting remote=true or remote=false on the iframe.
//
// This bug arises when we are default-in-process and a OOP iframe calls
// window.open, or when we're default-OOP and an in-process iframe calls
// window.open.  In either case, if the opened iframe gets the default
// remotness, it will not match its opener's remoteness, which is bad.
//
// Since the name of the test determines the OOP-by-default pref, the "inproc"
// version of this test opens an OOP frame, and the "oop" version opens an
// in-process frame.  Enjoy.  :)

"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addPermission();

  // We're going to open a remote frame if OOP off by default.  If OOP is on by
  // default, we're going to open an in-process frame.
  var remote = !browserElementTestHelpers.getOOPByDefaultPref();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  iframe.setAttribute('remote', remote);

  // The page we load does window.open, then checks some things and reports
  // back using alert().  Finally, it calls alert('finish').
  //
  // Bug 776129 in particular manifests itself such that the popup frame loads
  // and the tests in file_browserElement_OpenMixedProcess pass, but the
  // content of the frame is invisible.  To catch this case, we take a
  // screenshot after we load the content into the popup, and ensure that it's
  // not blank.
  var popup;
  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    popup = document.body.appendChild(e.detail.frameElement);
  });

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    if (e.detail.message.startsWith('pass')) {
      ok(true, e.detail.message);
    }
    else if (e.detail.message.startsWith('fail')) {
      ok(false, e.detail.message);
    }
    else if (e.detail.message == 'finish') {
      // We assume here that iframe is completely blank, and spin until popup's
      // screenshot is not the same as iframe.
      iframe.getScreenshot().onsuccess = function(e) {
        test2(popup, e.target.result, popup);
      };
    }
    else {
      ok(false, e.detail.message, "Unexpected message!");
    }
  });

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_OpenMixedProcess.html';
}

var prevScreenshot;
function test2(popup, blankScreenshot) {
  // Take screenshots of popup until it doesn't equal blankScreenshot (or we
  // time out).
  popup.getScreenshot().onsuccess = function(e) {
    var screenshot = e.target.result;
    if (screenshot != blankScreenshot) {
      SimpleTest.finish();
      return;
    }

    if (screenshot != prevScreenshot) {
      prevScreenshot = screenshot;
      dump("Got screenshot: " + screenshot + "\n");
    }
    SimpleTest.executeSoon(function() { test2(popup, blankScreenshot) });
  };
}

runTest();
