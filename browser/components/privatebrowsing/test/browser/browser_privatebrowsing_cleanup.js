/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DOMAIN = "http://example.com/";
const PATH = "browser/browser/components/privatebrowsing/test/browser/";
const TOP_PAGE = DOMAIN + PATH + "empty_file.html";

add_task(async () => {
  let observerExited = {
    observe(aSubject, aTopic, aData) {
      ok(false, "Notification received!");
    },
  };
  Services.obs.addObserver(observerExited, "last-pb-context-exited");

  // Create a private browsing window.
  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let privateTab = privateWindow.gBrowser.selectedBrowser;
  BrowserTestUtils.loadURI(privateTab, TOP_PAGE);
  await BrowserTestUtils.browserLoaded(privateTab);

  let popup = BrowserTestUtils.waitForNewWindow();

  await SpecialPowers.spawn(privateTab, [], () => {
    content.window.open("empty_file.html", "_blank", "width=300,height=300");
  });

  popup = await popup;
  ok(!!popup, "Popup shown");

  await BrowserTestUtils.closeWindow(privateWindow);
  Services.obs.removeObserver(observerExited, "last-pb-context-exited");

  let notificationPromise = TestUtils.topicObserved("last-pb-context-exited");

  popup.close();

  await notificationPromise;
  ok(true, "Notification received!");
});
