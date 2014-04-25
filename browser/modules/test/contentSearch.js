/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const SERVICE_EVENT_TYPE = "ContentSearchService";
const CLIENT_EVENT_TYPE = "ContentSearchClient";

// Forward events from the in-content service to the test.
content.addEventListener(SERVICE_EVENT_TYPE, event => {
  sendAsyncMessage(TEST_MSG, event.detail);
});

// Forward messages from the test to the in-content service.
addMessageListener(TEST_MSG, msg => {
  content.dispatchEvent(
    new content.CustomEvent(CLIENT_EVENT_TYPE, {
      detail: msg.data,
    })
  );
});
