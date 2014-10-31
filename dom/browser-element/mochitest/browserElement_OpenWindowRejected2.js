/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 742944 - Do window.open from inside <iframe mozbrowser>.  But then
// reject the call.  This shouldn't cause problems (crashes, leaks).
//
// This is the same as OpenWindowRejected, except we "reject" the popup by not
// adding the iframe element to our DOM, instead of by not calling
// preventDefault() on the event.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    ok(e.detail.url.indexOf('does_not_exist.html') != -1,
       'Opened URL; got ' + e.detail.url);
    is(e.detail.name, '');
    is(e.detail.features, '');

    // Call preventDefault, but don't add the iframe to the DOM.  This still
    // amounts to rejecting the popup.
    e.preventDefault();
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
}

addEventListener('testready', runTest);
