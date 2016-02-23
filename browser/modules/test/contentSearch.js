/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const SERVICE_EVENT_TYPE = "ContentSearchService";
const CLIENT_EVENT_TYPE = "ContentSearchClient";

// Forward events from the in-content service to the test.
content.addEventListener(SERVICE_EVENT_TYPE, event => {
  // The event dispatch code in content.js clones the event detail into the
  // content scope. That's generally the right thing, but causes us to end
  // up with an XrayWrapper to it here, which will screw us up when trying to
  // serialize the object in sendAsyncMessage. Waive Xrays for the benefit of
  // the test machinery.
  sendAsyncMessage(TEST_MSG, Components.utils.waiveXrays(event.detail));
});

// Forward messages from the test to the in-content service.
addMessageListener(TEST_MSG, msg => {
  content.dispatchEvent(
    new content.CustomEvent(CLIENT_EVENT_TYPE, {
      detail: msg.data,
    })
  );
});
