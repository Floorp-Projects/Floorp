/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const URL = "http://example.org/";

// Keep track of title of service we chose to share with
let serviceName;
let sharedUrl;
let sharedTitle;

let mockShareData = [{
  name: "NSA",
  menuItemTitle: "National Security Agency",
  image: "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA" +
    "LAAAAAABAAEAAAICTAEAOw=="
}];

let stub = sinon.stub(BrowserPageActions.shareURL, "_sharingService").get(() => {
  return {
    getSharingProviders(url) {
      return mockShareData;
    },
    shareUrl(name, url, title) {
      serviceName = name;
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
  await BrowserTestUtils.withNewTab(URL, async () => {
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

    Assert.equal(serviceName, mockShareData[0].name,
                 "Shared the correct service name");
    Assert.equal(sharedUrl, "http://example.org/",
                 "Shared correct URL");
    Assert.equal(sharedTitle, "mochitest index /",
                 "Shared with the correct title");
  });
});

add_task(async function shareURLAddressBar() {
  await BrowserTestUtils.withNewTab(URL, async () => {
    // Open pageAction panel
    await promisePageActionPanelOpen();

    // Right click the Share button
    let contextMenuPromise = promisePanelShown("pageActionContextMenu");
    let shareURLButton = document.getElementById("pageAction-panel-shareURL");
    EventUtils.synthesizeMouseAtCenter(shareURLButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    // Click "Add to Address Bar"
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    let ctxMenuButton = document.querySelector("#pageActionContextMenu " +
                                               ".pageActionContextMenuItem");
    EventUtils.synthesizeMouseAtCenter(ctxMenuButton, {});
    await contextMenuPromise;

    // Wait for the Share button to be added
    await BrowserTestUtils.waitForCondition(() => {
      return document.getElementById("pageAction-urlbar-shareURL");
    }, "Waiting for the share url button to be added to url bar");


    // Press the Share button
    let shareButton = document.getElementById("pageAction-urlbar-shareURL");
    let viewPromise = promisePageActionPanelShown();
    EventUtils.synthesizeMouseAtCenter(shareButton, {});
    await viewPromise;

    // Ensure we have share providers
    let panel = document.getElementById("pageAction-urlbar-shareURL-subview-body");
    Assert.equal(panel.childNodes.length, 1, "Has correct share receivers");

    // Remove the Share URL button from the Address bar so we dont interfere
    // with future tests
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(shareButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    ctxMenuButton = document.querySelector("#pageActionContextMenu " +
                                           ".pageActionContextMenuItem");
    EventUtils.synthesizeMouseAtCenter(ctxMenuButton, {});
    await contextMenuPromise;
  });
});
