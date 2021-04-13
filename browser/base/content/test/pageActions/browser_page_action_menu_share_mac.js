/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const URL = "http://example.org/";

// Keep track of title of service we chose to share with
let serviceName, sharedUrl, sharedTitle;
let sharingPreferencesCalled = false;

let mockShareData = [
  {
    name: "NSA",
    menuItemTitle: "National Security Agency",
    image:
      "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA" +
      "LAAAAAABAAEAAAICTAEAOw==",
  },
];

let stub = sinon
  .stub(BrowserPageActions.shareURL, "_sharingService")
  .get(() => {
    return {
      getSharingProviders(url) {
        return mockShareData;
      },
      shareUrl(name, url, title) {
        serviceName = name;
        sharedUrl = url;
        sharedTitle = title;
      },
      openSharingPreferences() {
        sharingPreferencesCalled = true;
      },
    };
  });

registerCleanupFunction(async function() {
  stub.restore();
  EventUtils.synthesizeNativeMouseEvent({
    type: "mousemove",
    target: document.documentElement,
    offsetX: 0,
    offsetY: 0,
  });
  await PlacesUtils.history.clear();
});

add_task(async function shareURL() {
  await BrowserTestUtils.withNewTab(URL, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // Click Share URL.
    let shareURLButton = document.getElementById("pageAction-panel-shareURL");
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(shareURLButton, {});

    let view = await viewPromise;
    let body = document.getElementById(view.id + "-body");

    // We should see 1 receiver and one extra node for the "More..." button
    Assert.equal(body.children.length, 2, "Has correct share receivers");
    let shareButton = body.children[0];
    Assert.equal(shareButton.label, mockShareData[0].menuItemTitle);
    let hiddenPromise = promisePageActionPanelHidden();
    // Click on share, panel should hide and sharingService should be
    // given the title of service to share with
    EventUtils.synthesizeMouseAtCenter(shareButton, {});
    await hiddenPromise;

    Assert.equal(
      serviceName,
      mockShareData[0].name,
      "Shared the correct service name"
    );
    Assert.equal(sharedUrl, "http://example.org/", "Shared correct URL");
    Assert.equal(
      sharedTitle,
      "mochitest index /",
      "Shared with the correct title"
    );
  });
});

add_task(async function openSharingPreferences() {
  await BrowserTestUtils.withNewTab(URL, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // Click Share URL.
    let shareURLButton = document.getElementById("pageAction-panel-shareURL");
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(shareURLButton, {});

    let view = await viewPromise;
    let body = document.getElementById(view.id + "-body");

    // We should see 1 receiver and one extra node for the "More..." button
    Assert.equal(body.children.length, 2, "Has correct share receivers");
    let moreButton = body.children[1];
    let hiddenPromise = promisePageActionPanelHidden();
    // Click on the "more" button,  panel should hide and we should call
    // the sharingService function to open preferences
    EventUtils.synthesizeMouseAtCenter(moreButton, {});
    await hiddenPromise;

    Assert.equal(
      sharingPreferencesCalled,
      true,
      "We called openSharingPreferences"
    );
  });
});
