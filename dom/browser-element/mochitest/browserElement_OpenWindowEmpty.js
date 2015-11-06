/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1216937 - Test that window.open with null/empty URL should use
// about:blank as default

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

    is(e.detail.url, 'about:blank', "Popup's has correct URL");
    e.preventDefault();

    SimpleTest.finish();
  });

  iframe.src = 'file_browserElement_OpenWindowEmpty.html';
  document.body.appendChild(iframe);
}

addEventListener('testready', runTest);
