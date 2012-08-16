/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that clicking a link with _target=blank works.

"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addPermission();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;

  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    is(e.detail.url, 'http://example.com/');
    SimpleTest.finish();
  });

  iframe.src = "file_browserElement_TargetBlank.html";
  document.body.appendChild(iframe);
}

runTest();
