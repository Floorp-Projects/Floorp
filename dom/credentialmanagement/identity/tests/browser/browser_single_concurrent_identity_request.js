/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URL = "https://example.com/";

add_task(async function test_concurrent_identity_credential() {
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

  ContentTask.spawn(tab.linkedBrowser, null, requestCredential);

  let secondRequest = ContentTask.spawn(
    tab.linkedBrowser,
    null,
    requestCredential
  );

  let concurrentResponse = await secondRequest;
  ok(concurrentResponse, "expect a result from the second request.");
  ok(concurrentResponse.name, "expect a DOMException which must have a name.");
  is(
    concurrentResponse.name,
    "InvalidStateError",
    "Expected 'InvalidStateError', but got '" + concurrentResponse.name + "'"
  );

  // Close tabs.
  await BrowserTestUtils.removeTab(tab);
});
