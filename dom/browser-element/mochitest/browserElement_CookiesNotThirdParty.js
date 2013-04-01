/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 806127 - Test that cookies set by <iframe mozbrowser> are not considered
// third-party.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  const innerPage = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_CookiesNotThirdParty.html';

  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    if (e.detail.message == 'next') {
      iframe.src = innerPage + '?step=2';
      return;
    }

    if (e.detail.message.startsWith('success:')) {
      ok(true, e.detail.message);
      return;
    }

    if (e.detail.message.startsWith('failure:')) {
      ok(false, e.detail.message);
      return;
    }

    if (e.detail.message == 'finish') {
      SimpleTest.finish();
    }
  });

  // innerPage will set a cookie and then alert('next').  We'll load
  // innerPage?step=2.  That page will check that the cooke exists (despite the
  // fact that we've disabled third-party cookies) and alert('success:') or
  // alert('failure:'), as appropriate.  Finally, the page will
  // alert('finish');
  iframe.src = innerPage;
  document.body.appendChild(iframe);
}

// Disable third-party cookies for this test.
addEventListener('testready', function() {
  SpecialPowers.pushPrefEnv({'set': [['network.cookie.cookieBehavior', 1]]}, runTest);
});
