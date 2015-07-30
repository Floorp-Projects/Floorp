/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page with tracking elements that get blocked and make sure that a
// 'learn more' link shows up in the webconsole.

"use strict";

const TEST_URI = "http://tracking.example.org/browser/browser/devtools/webconsole/test/test-trackingprotection-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/Firefox/Privacy/Tracking_Protection";
const PREF = "privacy.trackingprotection.enabled";
const {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF);
  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(function* testMessagesAppear() {
  yield UrlClassifierTestUtils.addTestTrackers();
  Services.prefs.setBoolPref(PREF, true);

  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "Was blocked because tracking protection is enabled",
        text: "The resource at \"http://tracking.example.com/\" was blocked because tracking protection is enabled",
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
    ],
  });

  yield testClickOpenNewTab(hud, results[0]);
});

function testClickOpenNewTab(hud, match) {
  let warningNode = match.clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");
  return simulateMessageLinkClick(warningNode, LEARN_MORE_URI);
}
