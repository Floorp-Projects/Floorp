/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

// Keep track of title of service we chose to share with
let sharedTitle;
let sharedUrl;

let mockShareData = [{
  title: "NSA",
  menuItemTitle: "National Security Agency",
  image: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA" +
    "LAAAAAABAAEAAAICTAEAOw=="
}];

let stub = sinon.stub(BrowserPageActions.shareURL, "_sharingService").get(() => {
  return {
    getSharingProviders(url) {
      return mockShareData;
    },
    shareUrl(title, url) {
      sharedUrl = url;
      sharedTitle = title;
    }
  };
});

registerCleanupFunction(async function() {
  stub.restore();
  delete window.sinon;
  await EventUtils.synthesizeNativeMouseMove(document.documentElement, 0, 0);
  await PlacesUtils.history.clear();
});

add_task(async function shareURL() {
  // Open an actionable page so that the main page action button appears.  (It
  // does not appear on about:blank for example.)
  let url = "http://example.org/";

  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // Click Share URL.
    let shareURLButton = document.getElementById("pageAction-panel-shareURL");
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(shareURLButton, {});

    let view = await viewPromise;
    let body = document.getElementById(view.id + "-body");

    Assert.equal(body.childNodes.length, 1, "Has correct share receivers");
    let shareButton = body.childNodes[0];
    Assert.equal(shareButton.label, mockShareData[0].menuItemTitle);
    let hiddenPromise = promisePageActionPanelHidden();
    // Click on share, panel should hide and sharingService should be
    // given the title of service to share with
    EventUtils.synthesizeMouseAtCenter(shareButton, {});
    await hiddenPromise;

    Assert.equal(sharedTitle, mockShareData[0].title,
                 "Shared with the correct title");
    Assert.equal(sharedUrl, "http://example.org/",
                 "Shared correct URL");

  });
});
