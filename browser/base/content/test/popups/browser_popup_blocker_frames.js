/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

async function test_opening_blocked_popups(testURL) {
  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.disable_open_during_load", true]],
  });

  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURL);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [baseURL + "popup_blocker.html"],
    uri => {
      let iframe = content.document.createElement("iframe");
      iframe.id = "popupframe";
      iframe.src = uri;
      content.document.body.appendChild(iframe);
    }
  );

  // Wait for the popup-blocked notification.
  await TestUtils.waitForCondition(
    () =>
      gBrowser.getNotificationBox().getNotificationWithValue("popup-blocked"),
    "Waiting for the popup-blocked notification."
  );

  let popupTabs = [];
  function onTabOpen(event) {
    popupTabs.push(event.target);
  }
  gBrowser.tabContainer.addEventListener("TabOpen", onTabOpen);

  await SpecialPowers.pushPermissions([
    { type: "popup", allow: true, context: testURL },
  ]);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [baseURL + "popup_blocker.html"],
    uri => {
      content.document.getElementById("popupframe").remove();
      let iframe = content.document.createElement("iframe");
      iframe.id = "popupframe";
      iframe.src = uri;
      content.document.body.appendChild(iframe);
    }
  );

  await TestUtils.waitForCondition(
    () =>
      popupTabs.length == 2 &&
      popupTabs.every(
        aTab => aTab.linkedBrowser.currentURI.spec != "about:blank"
      ),
    "Waiting for two tabs to be opened."
  );

  ok(
    popupTabs[0].linkedBrowser.currentURI.spec.endsWith("popup_blocker_a.html"),
    "Popup a"
  );
  ok(
    popupTabs[1].linkedBrowser.currentURI.spec.endsWith("popup_blocker_b.html"),
    "Popup b"
  );

  await SpecialPowers.popPermissions();

  gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.document.getElementById("popupframe").remove();
  });

  BrowserTestUtils.removeTab(tab);
  for (let popup of popupTabs) {
    gBrowser.removeTab(popup);
  }
}

add_task(async function () {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await test_opening_blocked_popups("http://example.com/");
});

add_task(async function () {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await test_opening_blocked_popups("http://w3c-test.org/");
});
