/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 781320 - Test that the name in <iframe mozbrowser name="foo"> is
// forwarded down to remote mozbrowsers.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.setAttribute('name', 'foo');

  iframe.addEventListener("mozbrowseropenwindow", function(e) {
    ok(false, 'Got mozbrowseropenwindow, but should not have.');
  });

  iframe.addEventListener('mozbrowserlocationchange', function(e) {
    ok(true, "Got locationchange to " + e.detail.url);
    if (e.detail.url.endsWith("ForwardName.html#finish")) {
      SimpleTest.finish();
    }
  });

  // The file sends us messages via alert() that start with "success:" or
  // "failure:".
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    ok(e.detail.message.startsWith('success:'), e.detail.message);
  });

  document.body.appendChild(iframe);

  // This file does window.open('file_browserElement_ForwardName.html#finish',
  // 'foo');  That should open in the curent window, because the window should
  // be named foo.
  iframe.src = 'file_browserElement_ForwardName.html';
}

addEventListener('testready', runTest);
