"use strict";

/* global sinon, UIState */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

registerCleanupFunction(function() {
  delete window.sinon;
});

const lastModifiedFixture = 1507655615.87; // Approx Oct 10th 2017
const mockRemoteClients = [
  { id: "0", name: "foo", type: "mobile", serverLastModified: lastModifiedFixture },
  { id: "1", name: "bar", type: "desktop", serverLastModified: lastModifiedFixture },
  { id: "2", name: "baz", type: "mobile", serverLastModified: lastModifiedFixture },
];

add_task(async function init() {
  // Disable panel animations.  They cause intermittent timeouts on Linux when
  // the test tries to synthesize clicks on items in newly opened panels.
  BrowserPageActions._disablePanelAnimations = true;
  registerCleanupFunction(() => {
    BrowserPageActions._disablePanelAnimations = false;
  });
});

add_task(async function bookmark() {
  // Open a unique page.
  let url = "http://example.com/browser_page_action_menu";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel.
    await promisePageActionPanelOpen();

    // The bookmark button should read "Bookmark This Page" and not be starred.
    let bookmarkButton = document.getElementById("pageAction-panel-bookmark");
    Assert.equal(bookmarkButton.label, "Bookmark This Page");
    Assert.ok(!bookmarkButton.hasAttribute("starred"));

    // Click the button.
    let hiddenPromise = promisePageActionPanelHidden();
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {});
    await hiddenPromise;

    // Make sure the edit-bookmark panel opens, then hide it.
    await new Promise(resolve => {
      if (StarUI.panel.state == "open") {
        resolve();
        return;
      }
      StarUI.panel.addEventListener("popupshown", resolve, { once: true });
    });
    Assert.equal(BookmarkingUI.starBox.getAttribute("open"), "true",
      "Star has open attribute");
    StarUI.panel.hidePopup();
    Assert.ok(!BookmarkingUI.starBox.hasAttribute("open"),
      "Star no longer has open attribute");

    // Open the panel again.
    await promisePageActionPanelOpen();

    // The bookmark button should now read "Edit This Bookmark" and be starred.
    Assert.equal(bookmarkButton.label, "Edit This Bookmark");
    Assert.ok(bookmarkButton.hasAttribute("starred"));
    Assert.equal(bookmarkButton.getAttribute("starred"), "true");

    // Click it again.
    hiddenPromise = promisePageActionPanelHidden();
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {});
    await hiddenPromise;

    // The edit-bookmark panel should open again.
    await new Promise(resolve => {
      if (StarUI.panel.state == "open") {
        resolve();
        return;
      }
      StarUI.panel.addEventListener("popupshown", resolve, { once: true });
    });

    let onItemRemovedPromise = PlacesTestUtils.waitForNotification("onItemRemoved",
      (id, parentId, index, type, itemUrl) => url == itemUrl.spec);

    // Click the remove-bookmark button in the panel.
    StarUI._element("editBookmarkPanelRemoveButton").click();

    // Wait for the bookmark to be removed before continuing.
    await onItemRemovedPromise;

    // Open the panel again.
    await promisePageActionPanelOpen();

    // The bookmark button should read "Bookmark This Page" and not be starred.
    Assert.equal(bookmarkButton.label, "Bookmark This Page");
    Assert.ok(!bookmarkButton.hasAttribute("starred"));

    // Done.
    hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;
  });
});

add_task(async function emailLink() {
  // Open an actionable page so that the main page action button appears.  (It
  // does not appear on about:blank for example.)
  let url = "http://example.com/";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Replace the email-link entry point to check whether it's called.
    let originalFn = MailIntegration.sendLinkForBrowser;
    let fnCalled = false;
    MailIntegration.sendLinkForBrowser = () => {
      fnCalled = true;
    };
    registerCleanupFunction(() => {
      MailIntegration.sendLinkForBrowser = originalFn;
    });

    // Open the panel and click Email Link.
    await promisePageActionPanelOpen();
    let emailLinkButton =
      document.getElementById("pageAction-panel-emailLink");
    let hiddenPromise = promisePageActionPanelHidden();
    EventUtils.synthesizeMouseAtCenter(emailLinkButton, {});
    await hiddenPromise;

    Assert.ok(fnCalled);
  });
});

add_task(async function copyURLFromPanel() {
  // Open an actionable page so that the main page action button appears.  (It
  // does not appear on about:blank for example.)
  let url = "http://example.com/";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel and click Copy URL.
    await promisePageActionPanelOpen();
    Assert.ok(true, "page action panel opened");

    let copyURLButton =
      document.getElementById("pageAction-panel-copyURL");
    let hiddenPromise = promisePageActionPanelHidden();
    EventUtils.synthesizeMouseAtCenter(copyURLButton, {});
    await hiddenPromise;

    let feedbackPanel = document.getElementById("pageActionFeedback");
    let feedbackShownPromise = BrowserTestUtils.waitForEvent(feedbackPanel, "popupshown");
    await feedbackShownPromise;
    Assert.equal(feedbackPanel.anchorNode.id, "pageActionButton", "Feedback menu should be anchored on the main Page Action button");
    let feedbackHiddenPromise = promisePanelHidden("pageActionFeedback");
    await feedbackHiddenPromise;
  });
});

add_task(async function copyURLFromURLBar() {
  // Open an actionable page so that the main page action button appears.  (It
  // does not appear on about:blank for example.)
  let url = "http://example.com/";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Add action to URL bar.
    let action = PageActions._builtInActions.find(a => a.id == "copyURL");
    action.pinnedToUrlbar = true;
    registerCleanupFunction(() => action.pinnedToUrlbar = false);

    let copyURLButton =
      document.getElementById("pageAction-urlbar-copyURL");
    let feedbackShownPromise = promisePanelShown("pageActionFeedback");
    EventUtils.synthesizeMouseAtCenter(copyURLButton, {});

    await feedbackShownPromise;
    let panel = document.getElementById("pageActionFeedback");
    Assert.equal(panel.anchorNode.id, "pageAction-urlbar-copyURL", "Feedback menu should be anchored on the main URL bar button");
    let feedbackHiddenPromise = promisePanelHidden("pageActionFeedback");
    await feedbackHiddenPromise;
  });
});

add_task(async function sendToDevice_nonSendable() {
  // Open a tab that's not sendable but where the page action buttons still
  // appear.  about:about is convenient.
  await BrowserTestUtils.withNewTab("about:about", async () => {
    await promiseSyncReady();
    // Open the panel.  Send to Device should be disabled.
    await promisePageActionPanelOpen();
    Assert.equal(BrowserPageActions.mainButtonNode.getAttribute("open"), "true",
                 "Main button has 'open' attribute");
    let panelButton =
      BrowserPageActions.panelButtonNodeForActionID("sendToDevice");
    Assert.equal(panelButton.disabled, true,
                 "The panel button should be disabled");
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;
    Assert.ok(!BrowserPageActions.mainButtonNode.hasAttribute("open"),
              "Main button no longer has 'open' attribute");
    // The urlbar button shouldn't exist.
    let urlbarButton =
      BrowserPageActions.urlbarButtonNodeForActionID("sendToDevice");
    Assert.equal(urlbarButton, null, "The urlbar button shouldn't exist");
  });
});

add_task(async function sendToDevice_syncNotReady_other_states() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    const sandbox = sinon.sandbox.create();
    sandbox.stub(gSync, "syncReady").get(() => false);
    sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => 0);
    sandbox.stub(UIState, "get").returns({ status: UIState.STATUS_NOT_VERIFIED });
    sandbox.stub(gSync, "isSendableURI").returns(true);

    let cleanUp = () => {
      sandbox.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "pageAction-panel-sendToDevice-subview");

    let expectedItems = [
      {
        id: "pageAction-panel-sendToDevice-notReady",
        display: "none",
        disabled: true,
      },
      {
        attrs: {
          label: "Account Not Verified",
        },
        disabled: true
      },
      null,
      {
        attrs: {
          label: "Verify Your Account...",
        },
      }
    ];
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;

    cleanUp();
  });
});

add_task(async function sendToDevice_syncNotReady_configured() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    const sandbox = sinon.sandbox.create();
    const syncReady = sandbox.stub(gSync, "syncReady").get(() => false);
    const lastSync = sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => 0);
    sandbox.stub(UIState, "get").returns({ status: UIState.STATUS_SIGNED_IN });
    sandbox.stub(gSync, "isSendableURI").returns(true);

    sandbox.stub(Weave.Service, "sync").callsFake(() => {
      syncReady.get(() => true);
      lastSync.get(() => Date.now());
      sandbox.stub(gSync, "remoteClients").get(() => mockRemoteClients);
    });

    let onShowingSubview = BrowserPageActions.sendToDevice.onShowingSubview;
    sandbox.stub(BrowserPageActions.sendToDevice, "onShowingSubview").callsFake((...args) => {
      this.numCall++ || (this.numCall = 1);
      onShowingSubview.call(BrowserPageActions.sendToDevice, ...args);
      testSendTabToDeviceMenu(this.numCall);
    });

    let cleanUp = () => {
      sandbox.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "pageAction-panel-sendToDevice-subview");

    function testSendTabToDeviceMenu(numCall) {
      if (numCall == 1) {
        // "Syncing devices" should be shown.
        checkSendToDeviceItems([
          {
            id: "pageAction-panel-sendToDevice-notReady",
            disabled: true,
          },
        ]);
      } else if (numCall == 2) {
        // The devices should be shown in the subview.
        let expectedItems = [
          {
            id: "pageAction-panel-sendToDevice-notReady",
            display: "none",
            disabled: true,
          },
        ];
        for (let client of mockRemoteClients) {
          expectedItems.push({
            attrs: {
              clientId: client.id,
              label: client.name,
              clientType: client.type,
              tooltiptext: gSync.formatLastSyncDate(lastModifiedFixture * 1000)
            },
          });
        }
        expectedItems.push(
          null,
          {
            attrs: {
              label: "Send to All Devices"
            }
          }
        );
        checkSendToDeviceItems(expectedItems);
      } else {
        ok(false, "This should never happen");
      }
    }

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;
    cleanUp();
  });
});

add_task(async function sendToDevice_notSignedIn() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();

    // Open the panel.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "pageAction-panel-sendToDevice-subview");

    let expectedItems = [
      {
        id: "pageAction-panel-sendToDevice-notReady",
        display: "none",
        disabled: true,
      },
      {
        attrs: {
          label: "Not Connected to Sync",
        },
        disabled: true
      },
      null,
      {
        attrs: {
          label: "Sign in to Sync..."
        },
      },
      {
        attrs: {
          label: "Learn About Sending Tabs..."
        },
      }
    ];
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;
  });
});

add_task(async function sendToDevice_noDevices() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    const sandbox = sinon.sandbox.create();
    sandbox.stub(gSync, "syncReady").get(() => true);
    sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => Date.now());
    sandbox.stub(UIState, "get").returns({ status: UIState.STATUS_SIGNED_IN });
    sandbox.stub(gSync, "isSendableURI").returns(true);
    sandbox.stub(gSync, "remoteClients").get(() => []);

    let cleanUp = () => {
      sandbox.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "pageAction-panel-sendToDevice-subview");

    let expectedItems = [
      {
        id: "pageAction-panel-sendToDevice-notReady",
        display: "none",
        disabled: true,
      },
      {
        attrs: {
          label: "No Devices Connected",
        },
        disabled: true
      },
      null,
      {
        attrs: {
          label: "Connect Another Device..."
        }
      },
      {
        attrs: {
          label: "Learn About Sending Tabs..."
        }
      }
    ];
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;

    cleanUp();

    await UIState.reset();
  });
});

add_task(async function sendToDevice_devices() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    const sandbox = sinon.sandbox.create();
    sandbox.stub(gSync, "syncReady").get(() => true);
    sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => Date.now());
    sandbox.stub(UIState, "get").returns({ status: UIState.STATUS_SIGNED_IN });
    sandbox.stub(gSync, "isSendableURI").returns(true);
    sandbox.stub(gSync, "remoteClients").get(() => mockRemoteClients);

    let cleanUp = () => {
      sandbox.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promisePageActionViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "pageAction-panel-sendToDevice-subview");

    // The devices should be shown in the subview.
    let expectedItems = [
      {
        id: "pageAction-panel-sendToDevice-notReady",
        display: "none",
        disabled: true,
      },
    ];
    for (let client of mockRemoteClients) {
      expectedItems.push({
        attrs: {
          clientId: client.id,
          label: client.name,
          clientType: client.type,
        },
      });
    }
    expectedItems.push(
      null,
      {
        attrs: {
          label: "Send to All Devices"
        }
      }
    );
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    BrowserPageActions.panelNode.hidePopup();
    await hiddenPromise;

    cleanUp();
  });
});

add_task(async function sendToDevice_inUrlbar() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    const sandbox = sinon.sandbox.create();
    sandbox.stub(gSync, "syncReady").get(() => true);
    sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => Date.now());
    sandbox.stub(UIState, "get").returns({ status: UIState.STATUS_SIGNED_IN });
    sandbox.stub(gSync, "isSendableURI").returns(true);
    sandbox.stub(gSync, "remoteClients").get(() => mockRemoteClients);

    let cleanUp = () => {
      sandbox.restore();
    };
    registerCleanupFunction(cleanUp);

    // Add Send to Device to the urlbar.
    let action = PageActions.actionForID("sendToDevice");
    action.pinnedToUrlbar = true;

    // Click it to open its panel.
    let urlbarButton = document.getElementById(
      BrowserPageActions.urlbarButtonNodeIDForActionID(action.id)
    );
    Assert.notEqual(urlbarButton, null, "The urlbar button should exist");
    Assert.ok(!urlbarButton.disabled,
              "The urlbar button should not be disabled");
    let panelPromise =
      promisePanelShown(BrowserPageActions._activatedActionPanelID);
    EventUtils.synthesizeMouseAtCenter(urlbarButton, {});
    await panelPromise;
    Assert.equal(urlbarButton.getAttribute("open"), "true",
      "Button has open attribute");

    // The devices should be shown in the subview.
    let expectedItems = [
      {
        id: "pageAction-urlbar-sendToDevice-notReady",
        display: "none",
        disabled: true,
      },
    ];
    for (let client of mockRemoteClients) {
      expectedItems.push({
        attrs: {
          clientId: client.id,
          label: client.name,
          clientType: client.type,
        },
      });
    }
    expectedItems.push(
      null,
      {
        attrs: {
          label: "Send to All Devices"
        }
      }
    );
    checkSendToDeviceItems(expectedItems, true);

    // Get the first device menu item in the panel.
    let bodyID =
      BrowserPageActions._panelViewNodeIDForActionID("sendToDevice", true) +
      "-body";
    let body = document.getElementById(bodyID);
    let deviceMenuItem = body.querySelector(".sendtab-target");
    Assert.notEqual(deviceMenuItem, null);

    // For good measure, wait until it's visible.
    let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
    await BrowserTestUtils.waitForCondition(() => {
      let bounds = dwu.getBoundsWithoutFlushing(deviceMenuItem);
      return bounds.height > 0 && bounds.width > 0;
    }, "Waiting for first device menu item to appear");

    // Click it, which should cause the panel to close.
    let hiddenPromise =
      promisePanelHidden(BrowserPageActions._activatedActionPanelID);
    EventUtils.synthesizeMouseAtCenter(deviceMenuItem, {});
    info("Waiting for Send to Device panel to close after clicking a device");
    await hiddenPromise;
    Assert.ok(!urlbarButton.hasAttribute("open"),
      "URL bar button no longer has open attribute");

    // And then the "Sent!" notification panel should open and close by itself
    // after a moment.
    info("Waiting for the Sent! notification panel to open");
    await promisePanelShown(BrowserPageActionFeedback.panelNode.id);
    Assert.equal(
      BrowserPageActionFeedback.panelNode.anchorNode.id,
      urlbarButton.id
    );
    info("Waiting for the Sent! notification panel to close");
    await promisePanelHidden(BrowserPageActionFeedback.panelNode.id);

    // Remove Send to Device from the urlbar.
    action.pinnedToUrlbar = false;

    cleanUp();
  });
});

add_task(async function contextMenu() {
  // Open an actionable page so that the main page action button appears.
  let url = "http://example.com/";
  await BrowserTestUtils.withNewTab(url, async () => {
    // Open the panel and then open the context menu on the bookmark button.
    await promisePageActionPanelOpen();
    let bookmarkButton = document.getElementById("pageAction-panel-bookmark");
    let contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    // The context menu should show the "remove" item.  Click it.
    let menuItems = collectContextMenuItems();
    Assert.equal(menuItems.length, 1,
                 "Context menu has one child");
    Assert.equal(menuItems[0].label, "Remove from Address Bar",
                 "Context menu is in the 'remove' state");
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be removed from the urlbar.  In this case, the bookmark
    // star, the node in the urlbar should be hidden.
    let starButtonBox = document.getElementById("star-button-box");
    await BrowserTestUtils.waitForCondition(() => {
      return starButtonBox.hidden;
    }, "Waiting for star button to become hidden");

    // Open the context menu again on the bookmark button.  (The page action
    // panel remains open.)
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    // The context menu should show the "add" item.  Click it.
    menuItems = collectContextMenuItems();
    Assert.equal(menuItems.length, 1,
                 "Context menu has one child");
    Assert.equal(menuItems[0].label, "Add to Address Bar",
                 "Context menu is in the 'add' state");
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be added to the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return !starButtonBox.hidden;
    }, "Waiting for star button to become unhidden");

    // Open the context menu on the bookmark star in the urlbar.
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(starButtonBox, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    // The context menu should show the "remove" item.  Click it.
    menuItems = collectContextMenuItems();
    Assert.equal(menuItems.length, 1,
                 "Context menu has one child");
    Assert.equal(menuItems[0].label, "Remove from Address Bar",
                 "Context menu is in the 'remove' state");
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;

    // The action should be removed from the urlbar.
    await BrowserTestUtils.waitForCondition(() => {
      return starButtonBox.hidden;
    }, "Waiting for star button to become hidden");

    // Finally, add the bookmark star back to the urlbar so that other tests
    // that rely on it are OK.
    await promisePageActionPanelOpen();
    contextMenuPromise = promisePanelShown("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(bookmarkButton, {
      type: "contextmenu",
      button: 2,
    });
    await contextMenuPromise;

    menuItems = collectContextMenuItems();
    Assert.equal(menuItems.length, 1,
                 "Context menu has one child");
    Assert.equal(menuItems[0].label, "Add to Address Bar",
                 "Context menu is in the 'add' state");
    contextMenuPromise = promisePanelHidden("pageActionContextMenu");
    EventUtils.synthesizeMouseAtCenter(menuItems[0], {});
    await contextMenuPromise;
    await BrowserTestUtils.waitForCondition(() => {
      return !starButtonBox.hidden;
    }, "Waiting for star button to become unhidden");
  });

  // urlbar tests that run after this one can break if the mouse is left over
  // the area where the urlbar popup appears, which seems to happen due to the
  // above synthesized mouse events.  Move it over the urlbar.
  EventUtils.synthesizeMouseAtCenter(gURLBar, { type: "mousemove" });
  gURLBar.focus();
});


function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
  return service.whenLoaded().then(() => {
    UIState.isReady();
    return UIState.refresh();
  });
}

function checkSendToDeviceItems(expectedItems, forUrlbar = false) {
  let bodyID =
    BrowserPageActions._panelViewNodeIDForActionID("sendToDevice", forUrlbar) +
    "-body";
  let body = document.getElementById(bodyID);
  Assert.equal(body.childNodes.length, expectedItems.length);
  for (let i = 0; i < expectedItems.length; i++) {
    let expected = expectedItems[i];
    let actual = body.childNodes[i];
    if (!expected) {
      Assert.equal(actual.localName, "toolbarseparator");
      continue;
    }
    if ("id" in expected) {
      Assert.equal(actual.id, expected.id);
    }
    let display = "display" in expected ? expected.display : "-moz-box";
    Assert.equal(getComputedStyle(actual).display, display);
    let disabled = "disabled" in expected ? expected.disabled : false;
    Assert.equal(actual.disabled, disabled);
    if ("attrs" in expected) {
      for (let name in expected.attrs) {
        Assert.ok(actual.hasAttribute(name));
        let attrVal = actual.getAttribute(name);
        if (name == "label") {
          attrVal = attrVal.normalize("NFKC"); // There's a bug with …
        }
        Assert.equal(attrVal, expected.attrs[name]);
      }
    }
  }
}

function collectContextMenuItems() {
  let contextMenu = document.getElementById("pageActionContextMenu");
  return Array.filter(contextMenu.childNodes, node => {
    return window.getComputedStyle(node).visibility == "visible";
  });
}
