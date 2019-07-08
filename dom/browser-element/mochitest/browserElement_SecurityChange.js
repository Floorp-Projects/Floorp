/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 763694 - Test that <iframe mozbrowser> delivers proper
// mozbrowsersecuritychange events.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");

  var lastSecurityState;
  iframe.addEventListener("mozbrowsersecuritychange", function(e) {
    lastSecurityState = e.detail;
  });

  var filepath =
    "tests/dom/browser-element/mochitest/file_browserElement_SecurityChange.html";

  var count = 0;
  iframe.addEventListener("mozbrowserloadend", function(e) {
    count++;
    switch (count) {
      case 1:
        is(lastSecurityState.state, "secure");
        is(lastSecurityState.extendedValidation, false);
        is(lastSecurityState.mixedContent, false);
        iframe.src = "http://example.com/" + filepath;
        break;
      case 2:
        is(lastSecurityState.state, "insecure");
        is(lastSecurityState.extendedValidation, false);
        is(lastSecurityState.mixedContent, false);
        iframe.src = "https://example.com:443/" + filepath + "?broken";
        break;
      case 3:
        is(lastSecurityState.state, "broken");
        is(lastSecurityState.extendedValidation, false);
        is(lastSecurityState.mixedContent, true);
        SimpleTest.finish();
        break;
    }
  });

  iframe.src = "https://example.com/" + filepath;
  document.body.appendChild(iframe);
}

addEventListener("testready", function() {
  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["browser.safebrowsing.phishing.enabled", false],
        ["browser.safebrowsing.malware.enabled", false],
      ],
    },
    runTest
  );
});
