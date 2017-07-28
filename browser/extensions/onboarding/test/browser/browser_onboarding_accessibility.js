/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

add_task(async function test_onboarding_overlay_button() {
  resetOnboardingDefaultState();

  info("Wait for onboarding overlay loaded");
  let tab = await openTab(ABOUT_HOME_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);

  info("Test accessibility and semantics of the overlay button");
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let doc = content.document;
    let button = doc.body.firstChild;
    is(button.id, "onboarding-overlay-button",
      "First child is an overlay button");
    ok(button.getAttribute("aria-label"),
      "Onboarding button has an accessible label");
    is(button.getAttribute("aria-haspopup"), "true",
      "Onboarding button should indicate that it triggers a popup");
    is(button.getAttribute("aria-controls"), "onboarding-overlay-dialog",
      "Onboarding button semantically controls an overlay dialog");
    is(button.firstChild.getAttribute("role"), "presentation",
      "Onboarding button icon should have presentation only semantics");
  });

  await BrowserTestUtils.removeTab(tab);
});
