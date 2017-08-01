"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

registerCleanupFunction(function() {
  delete window.sinon;
});

Cu.import("resource://services-sync/UIState.jsm");

const mockRemoteClients = [
  { id: "0", name: "foo", type: "mobile" },
  { id: "1", name: "bar", type: "desktop" },
  { id: "2", name: "baz", type: "mobile" },
];

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
    StarUI.panel.hidePopup();

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
    gPageActionPanel.hidePopup();
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

add_task(async function sendToDevice_nonSendable() {
  // Open a tab that's not sendable -- but that's also actionable so that the
  // main page action button appears.
  await BrowserTestUtils.withNewTab("about:home", async () => {
    await promiseSyncReady();
    // Open the panel.  Send to Device should be disabled.
    await promisePageActionPanelOpen();
    let sendToDeviceButton =
      document.getElementById("pageAction-panel-sendToDevice");
    Assert.ok(sendToDeviceButton.disabled);
    let hiddenPromise = promisePageActionPanelHidden();
    gPageActionPanel.hidePopup();
    await hiddenPromise;
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
    gPageActionPanel.hidePopup();
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
    gPageActionPanel.hidePopup();
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
          label: "Learn About Sending Tabs..."
        },
      }
    ];
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    gPageActionPanel.hidePopup();
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
          label: "Learn About Sending Tabs..."
        }
      }
    ];
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePageActionPanelHidden();
    gPageActionPanel.hidePopup();
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
    gPageActionPanel.hidePopup();
    await hiddenPromise;

    cleanUp();
  });
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

function checkSendToDeviceItems(expectedItems) {
  let body =
    document.getElementById("pageAction-panel-sendToDevice-subview-body");
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
        let attrVal = actual.getAttribute(name)
        if (name == "label") {
          attrVal = attrVal.normalize("NFKC"); // There's a bug with …
        }
        Assert.equal(attrVal, expected.attrs[name]);
      }
    }
  }
}
