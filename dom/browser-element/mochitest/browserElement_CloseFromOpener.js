/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that window.close() works from the opener window.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    ok(true, "got openwindow event.");
    document.body.appendChild(e.detail.frameElement);

    e.detail.frameElement.addEventListener("mozbrowserclose", function(e) {
      ok(true, "got mozbrowserclose event.");
      SimpleTest.finish();
    });
  });


  document.body.appendChild(iframe);

  // file_browserElement_CloseFromOpener opens a new window and then calls
  // close() on it.
  iframe.src = "file_browserElement_CloseFromOpener.html";
}

addEventListener('testready', runTest);
