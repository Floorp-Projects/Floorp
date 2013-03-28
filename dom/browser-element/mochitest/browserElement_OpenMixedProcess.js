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
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  // We're going to open a remote frame if OOP off by default.  If OOP is on by
  // default, we're going to open an in-process frame.
  var remote = !browserElementTestHelpers.getOOPByDefaultPref();

  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
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
      iframe.getScreenshot(1000, 1000).onsuccess = function(e) {
        var fr = FileReader();
        fr.onloadend = function() { test2(popup, fr.result); };
        fr.readAsArrayBuffer(e.target.result);
      };
    }
    else {
      ok(false, e.detail.message, "Unexpected message!");
    }
  });

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_OpenMixedProcess.html';
}

function arrayBuffersEqual(a, b) {
  var x = new Int8Array(a);
  var y = new Int8Array(b);
  if (x.length != y.length) {
    return false;
  }

  for (var i = 0; i < x.length; i++) {
    if (x[i] != y[i]) {
      return false;
    }
  }

  return true;
}

function test2(popup, blankScreenshotArrayBuffer) {
  // Take screenshots of popup until it doesn't equal blankScreenshot (or we
  // time out).
  popup.getScreenshot(1000, 1000).onsuccess = function(e) {
    var fr = new FileReader();
    fr.onloadend = function() {
      if (!arrayBuffersEqual(blankScreenshotArrayBuffer, fr.result)) {
        ok(true, "Finally got a non-blank screenshot.");
        SimpleTest.finish();
        return;
      }

      SimpleTest.executeSoon(function() { test2(popup, blankScreenshot) });
    };
    fr.readAsArrayBuffer(e.target.result);
  };
}

addEventListener('testready', runTest);
