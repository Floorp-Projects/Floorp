"use strict";

let gPanel = document.getElementById("page-action-panel");

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
    await promisePanelOpen();

    // The bookmark button should read "Bookmark This Page" and not be starred.
    let bookmarkButton = document.getElementById("page-action-bookmark-button");
    Assert.equal(bookmarkButton.label, "Bookmark This Page");
    Assert.ok(!bookmarkButton.hasAttribute("starred"));

    // Click the button.
    let hiddenPromise = promisePanelHidden();
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
    await promisePanelOpen();

    // The bookmark button should now read "Edit This Bookmark" and be starred.
    Assert.equal(bookmarkButton.label, "Edit This Bookmark");
    Assert.ok(bookmarkButton.hasAttribute("starred"));
    Assert.equal(bookmarkButton.getAttribute("starred"), "true");

    // Click it again.
    hiddenPromise = promisePanelHidden();
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

    // Click the remove-bookmark button in the panel.
    StarUI._element("editBookmarkPanelRemoveButton").click();

    // Open the panel again.
    await promisePanelOpen();

    // The bookmark button should read "Bookmark This Page" and not be starred.
    Assert.equal(bookmarkButton.label, "Bookmark This Page");
    Assert.ok(!bookmarkButton.hasAttribute("starred"));

    // Done.
    hiddenPromise = promisePanelHidden();
    gPanel.hidePopup();
    await hiddenPromise;
  });
});

add_task(async function copyURL() {
  // Open the panel.
  await promisePanelOpen();

  // Click Copy URL.
  let copyURLButton = document.getElementById("page-action-copy-url-button");
  let hiddenPromise = promisePanelHidden();
  EventUtils.synthesizeMouseAtCenter(copyURLButton, {});
  await hiddenPromise;

  // Check the clipboard.
  let transferable = Cc["@mozilla.org/widget/transferable;1"]
                       .createInstance(Ci.nsITransferable);
  transferable.init(null);
  let flavor = "text/unicode";
  transferable.addDataFlavor(flavor);
  Services.clipboard.getData(transferable, Services.clipboard.kGlobalClipboard);
  let strObj = {};
  transferable.getTransferData(flavor, strObj, {});
  Assert.ok(!!strObj.value);
  strObj.value.QueryInterface(Ci.nsISupportsString);
  Assert.equal(strObj.value.data, gBrowser.selectedBrowser.currentURI.spec);
});

add_task(async function emailLink() {
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
  await promisePanelOpen();
  let emailLinkButton =
    document.getElementById("page-action-email-link-button");
  let hiddenPromise = promisePanelHidden();
  EventUtils.synthesizeMouseAtCenter(emailLinkButton, {});
  await hiddenPromise;

  Assert.ok(fnCalled);
});

add_task(async function sendToDevice_nonSendable() {
  // Open a tab that's not sendable.
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Open the panel.  Send to Device should be disabled.
    await promisePanelOpen();
    let sendToDeviceButton =
      document.getElementById("page-action-send-to-device-button");
    Assert.ok(sendToDeviceButton.disabled);
    let hiddenPromise = promisePanelHidden();
    gPanel.hidePopup();
    await hiddenPromise;
  });
});

add_task(async function sendToDevice_syncNotReady() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    let syncReadyMock = mockReturn(gSync, "syncReady", false);
    let signedInMock = mockReturn(gSync, "isSignedIn", true);

    let remoteClientsMock;
    let origSync = Weave.Service.sync;
    Weave.Service.sync = () => {
      mockReturn(gSync, "syncReady", true);
      remoteClientsMock = mockReturn(gSync, "remoteClients", mockRemoteClients);
    };

    let origSetupSendToDeviceView = gPageActionButton.setupSendToDeviceView;
    gPageActionButton.setupSendToDeviceView = () => {
      this.numCall++ || (this.numCall = 1);
      origSetupSendToDeviceView.call(gPageActionButton);
      testSendTabToDeviceMenu(this.numCall);
    }

    let cleanUp = () => {
      Weave.Service.sync = origSync;
      gPageActionButton.setupSendToDeviceView = origSetupSendToDeviceView;
      signedInMock.restore();
      syncReadyMock.restore();
      remoteClientsMock.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePanelOpen();
    let sendToDeviceButton =
      document.getElementById("page-action-send-to-device-button");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promiseViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "page-action-sendToDeviceView");

    function testSendTabToDeviceMenu(numCall) {
      if (numCall == 1) {
        // The Fxa button should be shown.
        checkSendToDeviceItems([
          {
            id: "page-action-sendToDevice-fxa-button",
            display: "none",
          },
          {
            id: "page-action-no-devices-button",
            display: "none",
            disabled: true,
          },
          {
            id: "page-action-sync-not-ready-button",
            disabled: true,
          },
        ]);
      } else if (numCall == 2) {
        // The devices should be shown in the subview.
        let expectedItems = [
          {
            id: "page-action-sendToDevice-fxa-button",
            display: "none",
          },
          {
            id: "page-action-no-devices-button",
            display: "none",
            disabled: true,
          },
          {
            id: "page-action-sync-not-ready-button",
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
            label: "Send to All Devices",
          }
        );
        checkSendToDeviceItems(expectedItems);
      } else {
        ok(false, "This should never happen");
      }
    }

    // Done, hide the panel.
    let hiddenPromise = promisePanelHidden();
    gPanel.hidePopup();
    await hiddenPromise;
    cleanUp();
  });
});

add_task(async function sendToDevice_notSignedIn() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();

    // Open the panel.
    await promisePanelOpen();
    let sendToDeviceButton =
      document.getElementById("page-action-send-to-device-button");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promiseViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "page-action-sendToDeviceView");

    // The Fxa button should be shown.
    checkSendToDeviceItems([
      {
        id: "page-action-sendToDevice-fxa-button",
      },
      {
        id: "page-action-no-devices-button",
        display: "none",
        disabled: true,
      },
      {
        id: "page-action-sync-not-ready-button",
        display: "none",
        disabled: true,
      },
    ]);

    // Click the Fxa button.
    let body = view.firstChild;
    let fxaButton = body.childNodes[0];
    Assert.equal(fxaButton.id, "page-action-sendToDevice-fxa-button");
    let prefsTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);
    let hiddenPromise = promisePanelHidden();
    EventUtils.synthesizeMouseAtCenter(fxaButton, {});
    let values = await Promise.all([prefsTabPromise, hiddenPromise]);
    let tab = values[0];

    // The Fxa prefs pane should open.  The full URL is something like:
    //   about:preferences?entrypoint=syncbutton#sync
    // Just make sure it's about:preferences#sync.
    let urlObj = new URL(gBrowser.selectedBrowser.currentURI.spec);
    let url = urlObj.protocol + urlObj.pathname + urlObj.hash;
    Assert.equal(url, "about:preferences#sync");

    await BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function sendToDevice_noDevices() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    UIState._internal._state = { status: UIState.STATUS_SIGNED_IN };

    // Open the panel.
    await promisePanelOpen();
    let sendToDeviceButton =
      document.getElementById("page-action-send-to-device-button");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promiseViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "page-action-sendToDeviceView");

    // The no-devices item should be shown.
    checkSendToDeviceItems([
      {
        id: "page-action-sendToDevice-fxa-button",
        display: "none",
      },
      {
        id: "page-action-no-devices-button",
        disabled: true,
      },
      {
        id: "page-action-sync-not-ready-button",
        display: "none",
        disabled: true,
      },
    ]);

    // Done, hide the panel.
    let hiddenPromise = promisePanelHidden();
    gPanel.hidePopup();
    await hiddenPromise;

    await UIState.reset();
  });
});

add_task(async function sendToDevice_devices() {
  // Open a tab that's sendable.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    await promiseSyncReady();
    UIState._internal._state = { status: UIState.STATUS_SIGNED_IN };

    // Set up mock remote clients.
    let remoteClientsMock = mockReturn(gSync, "remoteClients", mockRemoteClients);
    let cleanUp = () => {
      remoteClientsMock.restore();
    };
    registerCleanupFunction(cleanUp);

    // Open the panel.
    await promisePanelOpen();
    let sendToDeviceButton =
      document.getElementById("page-action-send-to-device-button");
    Assert.ok(!sendToDeviceButton.disabled);

    // Click Send to Device.
    let viewPromise = promiseViewShown();
    EventUtils.synthesizeMouseAtCenter(sendToDeviceButton, {});
    let view = await viewPromise;
    Assert.equal(view.id, "page-action-sendToDeviceView");

    // The devices should be shown in the subview.
    let expectedItems = [
      {
        id: "page-action-sendToDevice-fxa-button",
        display: "none",
      },
      {
        id: "page-action-no-devices-button",
        display: "none",
        disabled: true,
      },
      {
        id: "page-action-sync-not-ready-button",
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
        label: "Send to All Devices",
      }
    );
    checkSendToDeviceItems(expectedItems);

    // Done, hide the panel.
    let hiddenPromise = promisePanelHidden();
    gPanel.hidePopup();
    await hiddenPromise;

    cleanUp();
    await UIState.reset();
  });
});

function promisePanelOpen() {
  let button = document.getElementById("urlbar-page-action-button");
  let shownPromise = promisePanelShown();
  EventUtils.synthesizeMouseAtCenter(button, {});
  return shownPromise;
}

function promisePanelShown() {
  return promisePanelEvent("popupshown");
}

function promisePanelHidden() {
  return promisePanelEvent("popuphidden");
}

function promisePanelEvent(name) {
  return new Promise(resolve => {
    gPanel.addEventListener(name, () => {
      setTimeout(() => {
        resolve();
      });
    }, { once: true });
  });
}

function promiseViewShown() {
  return Promise.all([
    promiseViewShowing(),
    promiseTransitionEnd(),
  ]).then(values => {
    return new Promise(resolve => {
      setTimeout(() => {
        resolve(values[0]);
      });
    });
  });
}

function promiseViewShowing() {
  return new Promise(resolve => {
    gPanel.addEventListener("ViewShowing", (event) => {
      resolve(event.target);
    }, { once: true });
  });
}

function promiseTransitionEnd() {
  return new Promise(resolve => {
    gPanel.addEventListener("transitionend", (event) => {
      resolve(event.target);
    }, { once: true });
  });
}

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
  let body = document.getElementById("page-action-sendToDeviceView-body");
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
        Assert.equal(actual.getAttribute(name), expected.attrs[name]);
      }
    }
  }
}

// Copied from test/sync/head.js (see bug 1369855)
function mockReturn(obj, symbol, fixture) {
  let getter = Object.getOwnPropertyDescriptor(obj, symbol).get;
  if (getter) {
    Object.defineProperty(obj, symbol, {
      get() { return fixture; }
    });
    return {
      restore() {
        Object.defineProperty(obj, symbol, {
          get: getter
        });
      }
    }
  }
  let func = obj[symbol];
  obj[symbol] = () => fixture;
  return {
    restore() {
      obj[symbol] = func;
    }
  }
}
