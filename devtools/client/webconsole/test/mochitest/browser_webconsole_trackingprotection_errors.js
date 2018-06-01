/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a page with tracking elements that get blocked and make sure that a
// 'learn more' link shows up in the webconsole.

"use strict";

const TEST_URI = "http://tracking.example.org/browser/devtools/client/" +
                 "webconsole/test/mochitest/" +
                 "test-trackingprotection-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/Firefox/Privacy/" +
                       "Tracking_Protection" + DOCS_GA_PARAMS;

const {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function testMessagesAppear() {
  await UrlClassifierTestUtils.addTestTrackers();
  await pushPref("privacy.trackingprotection.enabled", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const message = await waitFor(() => findMessage(hud,
    "The resource at \u201chttp://tracking.example.com/\u201d was " +
    "blocked because tracking protection is enabled"));

  await testClickOpenNewTab(hud, message);
});

async function testClickOpenNewTab(hud, message) {
  info("Clicking on the Learn More link");

  const learnMoreLink = message.querySelector(".learn-more-link");
  const linkSimulation = await simulateLinkClick(learnMoreLink);
  checkLink({
    ...linkSimulation,
    expectedLink: LEARN_MORE_URI,
    expectedTab: "tab"
  });
}

function checkLink({ link, where, expectedLink, expectedTab }) {
  is(link, expectedLink, `Clicking the provided link opens ${link}`);
  is(where, expectedTab, `Clicking the provided link opens in expected tab`);
}
