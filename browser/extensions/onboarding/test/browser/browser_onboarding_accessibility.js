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
    let button = doc.body.firstElementChild;
    is(button.id, "onboarding-overlay-button",
      "First child is an overlay button");
    ok(button.getAttribute("aria-label"),
      "Onboarding button has an accessible label");
    is(button.getAttribute("aria-haspopup"), "true",
      "Onboarding button should indicate that it triggers a popup");
    is(button.getAttribute("aria-controls"), "onboarding-overlay-dialog",
      "Onboarding button semantically controls an overlay dialog");
    is(button.firstElementChild.getAttribute("role"), "presentation",
      "Onboarding button icon should have presentation only semantics");
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_onboarding_notification_bar() {
  resetOnboardingDefaultState();
  skipMuteNotificationOnFirstSession();

  let tab = await openTab(ABOUT_NEWTAB_URL);
  await promiseOnboardingOverlayLoaded(tab.linkedBrowser);
  await promiseTourNotificationOpened(tab.linkedBrowser);

  info("Test accessibility and semantics of the notification bar");
  await ContentTask.spawn(tab.linkedBrowser, {}, function() {
    let doc = content.document;
    let footer = doc.getElementById("onboarding-notification-bar");

    is(footer.getAttribute("aria-labelledby"), doc.getElementById("onboarding-notification-tour-title").id,
      "Notification bar should be labelled by the notification tour title text");

    is(footer.getAttribute("aria-live"), "polite",
      "Notification bar should be a live region");
    // Presentational elements
    [
      "onboarding-notification-message-section",
      "onboarding-notification-tour-icon",
      "onboarding-notification-body"
    ].forEach(id =>
      is(doc.getElementById(id).getAttribute("role"), "presentation",
        "Element is only used for presentation"));
  });

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_onboarding_overlay_dialog() {
  resetOnboardingDefaultState();

  info("Wait for onboarding overlay loaded");
  let tab = await openTab(ABOUT_HOME_URL);
  let browser = tab.linkedBrowser;
  await promiseOnboardingOverlayLoaded(browser);

  info("Test accessibility and semantics of the dialog overlay");
  await assertModalDialog(browser, { visible: false });

  info("Click on overlay button and check modal dialog state");
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-button",
                                                 {}, browser);
  await promiseOnboardingOverlayOpened(browser);
  await assertModalDialog(browser,
    { visible: true, focusedId: "onboarding-overlay-dialog" });

  info("Close the dialog and check modal dialog state");
  await BrowserTestUtils.synthesizeMouseAtCenter("#onboarding-overlay-close-btn",
                                                 {}, browser);
  await promiseOnboardingOverlayClosed(browser);
  await assertModalDialog(browser, { visible: false });

  BrowserTestUtils.removeTab(tab);
});
