/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const baseURL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(async function test_opening_blocked_popups() {
  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({set: [["dom.disable_open_during_load", true]]});

  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html,Hello");

  await ContentTask.spawn(tab.linkedBrowser, baseURL + "popup_blocker.html", uri => {
    let iframe = content.document.createElement("iframe");
    iframe.id = "popupframe";
    iframe.src = uri;
    content.document.body.appendChild(iframe);
  });

  // Wait for the popup-blocked notification.
  let notification;
  await BrowserTestUtils.waitForCondition(() =>
    notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"));

  ok(notification, "Should have notification.");
  ok(!document.getElementById("page-report-button").hidden,
     "URL bar popup indicator should not be hidden");

  await ContentTask.spawn(tab.linkedBrowser, baseURL, async function(uri) {
    let iframe = content.document.createElement("iframe");
    iframe.src = uri;
    let pageHideHappened = ContentTaskUtils.waitForEvent(this, "pagehide", true);
    content.document.body.appendChild(iframe);
    await pageHideHappened;
  });

  notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked");
  ok(notification, "Should still have notification");
  ok(!document.getElementById("page-report-button").hidden,
     "URL bar popup indicator should still be visible");

  // Now navigate the subframe.
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let pageHideHappened = ContentTaskUtils.waitForEvent(this, "pagehide", true);
    content.document.getElementById("popupframe").contentDocument.location.href = "about:blank";
    await pageHideHappened;
  });

  await BrowserTestUtils.waitForCondition(() =>
    !gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"));
  ok(!gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"),
     "Should no longer have notification");
  ok(document.getElementById("page-report-button").hidden,
     "URL bar popup indicator should be hidden");

  // Remove the frame and add another one:
  await ContentTask.spawn(tab.linkedBrowser, baseURL + "popup_blocker.html", uri => {
    content.document.getElementById("popupframe").remove();
    let iframe = content.document.createElement("iframe");
    iframe.id = "popupframe";
    iframe.src = uri;
    content.document.body.appendChild(iframe);
  });

  // Wait for the popup-blocked notification.
  await BrowserTestUtils.waitForCondition(() =>
    notification = gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"));

  ok(notification, "Should have notification.");
  ok(!document.getElementById("page-report-button").hidden,
     "URL bar popup indicator should not be hidden");

  await ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.document.getElementById("popupframe").remove();
  });

  await BrowserTestUtils.waitForCondition(() =>
    !gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"));
  ok(!gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"),
     "Should no longer have notification");
  ok(document.getElementById("page-report-button").hidden,
     "URL bar popup indicator should be hidden");

  await BrowserTestUtils.removeTab(tab);
});

