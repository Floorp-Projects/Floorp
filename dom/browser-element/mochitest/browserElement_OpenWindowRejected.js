/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 742944 - Do window.open from inside <iframe mozbrowser>.  But then
// reject the call.  This shouldn't cause problems (crashes, leaks).

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    ok(e.detail.url.indexOf('does_not_exist.html') != -1,
       'Opened URL; got ' + e.detail.url);
    is(e.detail.name, '');
    is(e.detail.features, '');

    // Don't add e.detail.frameElement to the DOM, so the window.open is
    // effectively blocked.
  });

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    var msg = e.detail.message;
    if (msg.indexOf('success:') == 0) {
      ok(true, msg);
    }
    else if (msg == 'finish') {
      SimpleTest.finish();
    }
    else {
      ok(false, msg);
    }
  });

  iframe.src = 'file_browserElement_OpenWindowRejected.html';
  document.body.appendChild(iframe);
}

addEventListener('testready', runTest);
