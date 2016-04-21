/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 762939 - Test that setting a <iframe mozbrowser> to invisible / visible
// inside an invisible <iframe mozbrowser> doesn't trigger any events.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  // We need remote = false here until bug 761935 is fixed; see
  // SetVisibleFrames.js for an explanation.
  iframe.remote = false;

  iframe.addEventListener('mozbrowserloadend', function loadEnd(e) {
    iframe.removeEventListener('mozbrowserloadend', loadEnd);
    iframe.setVisible(false);
    iframe.src = 'file_browserElement_SetVisibleFrames2_Outer.html';
  });

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    if (e.detail.message == 'parent:finish') {
      ok(true, "Got parent:finish");

      // Give any extra events a chance to fire, then end the test.
      SimpleTest.executeSoon(function() {
        SimpleTest.executeSoon(function() {
          SimpleTest.executeSoon(function() {
            SimpleTest.executeSoon(function() {
              SimpleTest.executeSoon(function() {
                finish();
              });
            });
          });
        });
      });
    }
    else {
      ok(false, "Got unexpected message: " + e.detail.message);
    }
  });

  document.body.appendChild(iframe);
}

function finish() {
  SimpleTest.finish();
}

addEventListener('testready', runTest);
