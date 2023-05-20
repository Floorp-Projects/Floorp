/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

add_task(async function test_close_prompt_on_timeout() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "dom.security.credentialmanagement.identity.reject_delay.duration_ms",
        1000,
      ],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let requestCredential = async function () {
    let promise = content.navigator.credentials.get({
      identity: {
        providers: [
          {
            configURL:
              "https://example.net/tests/dom/credentialmanagement/identity/tests/browser/server_manifest.json",
            clientId: "browser",
            nonce: "nonce",
          },
        ],
      },
    });
    try {
      return await promise;
    } catch (err) {
      return err;
    }
  };

  let popupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );

  let request = ContentTask.spawn(tab.linkedBrowser, null, requestCredential);

  await popupShown;
  await request;

  let notification = PopupNotifications.getNotification(
    "identity-credential",
    tab.linkedBrowser
  );
  ok(
    !notification,
    "Identity Credential notification must not be present after timeout."
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
