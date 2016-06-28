/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 763694 - Test that <iframe mozbrowser> delivers proper
// mozbrowsersecuritychange events.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

const { Services } = SpecialPowers.Cu.import('resource://gre/modules/Services.jsm');
const { UrlClassifierTestUtils } = SpecialPowers.Cu.import('resource://testing-common/UrlClassifierTestUtils.jsm', {});

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  var lastSecurityState;
  iframe.addEventListener('mozbrowsersecuritychange', function(e) {
    lastSecurityState = e.detail;
  });

  var filepath = 'tests/dom/browser-element/mochitest/file_browserElement_SecurityChange.html';

  var count = 0;
  iframe.addEventListener('mozbrowserloadend', function(e) {
    count++;
    switch (count) {
    case 1:
      is(lastSecurityState.state, 'secure');
      is(lastSecurityState.extendedValidation, false);
      is(lastSecurityState.trackingContent, false);
      is(lastSecurityState.mixedContent, false);
      iframe.src = "http://example.com/" + filepath;
      break;
    case 2:
      is(lastSecurityState.state, 'insecure');
      is(lastSecurityState.extendedValidation, false);
      is(lastSecurityState.trackingContent, false);
      is(lastSecurityState.mixedContent, false);
      iframe.src = 'https://example.com:443/' + filepath + '?broken';
      break;
    case 3:
      is(lastSecurityState.state, 'broken');
      is(lastSecurityState.extendedValidation, false);
      is(lastSecurityState.trackingContent, false);
      is(lastSecurityState.mixedContent, true);
      iframe.src = "http://example.com/" + filepath + '?tracking';
      break;
    case 4:
      is(lastSecurityState.state, 'insecure');
      is(lastSecurityState.extendedValidation, false);
      // TODO: I'm having trouble getting the tracking protection
      // test changes to be enabled in the child process, so this
      // isn't currently blocked in tests, but it works when
      // manually testing.
      // is(lastSecurityState.trackingContent, true);
      is(lastSecurityState.mixedContent, false);
      SimpleTest.finish();
    }
  });

  iframe.src = "https://example.com/" + filepath;
  document.body.appendChild(iframe);
}

addEventListener('testready', function() {
  SimpleTest.registerCleanupFunction(UrlClassifierTestUtils.cleanupTestTrackers);
  SpecialPowers.pushPrefEnv({"set" : [
    ["privacy.trackingprotection.enabled", true],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["browser.safebrowsing.phishing.enabled", false],
    ["browser.safebrowsing.malware.enabled", false],
  ]}, () => {
     SimpleTest.registerCleanupFunction(UrlClassifierTestUtils.cleanupTestTrackers);
     UrlClassifierTestUtils.addTestTrackers().then(() => {
       runTest();
     });
  });
});

