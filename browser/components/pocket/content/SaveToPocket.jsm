/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "Pocket",
  "chrome://pocket/content/Pocket.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AboutReaderParent",
  "resource:///actors/AboutReaderParent.jsm"
);

var EXPORTED_SYMBOLS = ["SaveToPocket"];

XPCOMUtils.defineLazyGetter(this, "gStrings", () => {
  return Services.strings.createBundle(
    "chrome://global/locale/aboutReader.properties"
  );
});

var PocketCustomizableWidget = {
  init() {
    CustomizableUI.createWidget({
      id: "save-to-pocket-button",
      l10nId: "save-to-pocket-button",
      type: "view",
      viewId: "PanelUI-savetopocket",
      // This closes any open Pocket panels if you change location.
      locationSpecific: true,
      onViewShowing(aEvent) {
        let panelView = aEvent.target;
        let panelNode = panelView.querySelector(
          ".PanelUI-savetopocket-container"
        );
        let doc = panelNode.ownerDocument;
        let frame = doc.createXULElement("browser");

        frame.setAttribute("type", "content");
        panelNode.appendChild(frame);

        SaveToPocket.onShownInToolbarPanel(panelNode, frame);
      },
      onViewHiding(aEvent) {
        let panelView = aEvent.target;
        let panelNode = panelView.querySelector(
          ".PanelUI-savetopocket-container"
        );

        panelNode.textContent = "";
        SaveToPocket.updateToolbarNodeState(panelNode.ownerGlobal);
      },
    });
  },
  shutdown() {
    CustomizableUI.destroyWidget("save-to-pocket-button");
  },
};

// PocketContextMenu
// When the context menu is opened check if we need to build and enable pocket UI.
var PocketContextMenu = {
  init() {
    Services.obs.addObserver(this, "on-build-contextmenu");
  },
  shutdown() {
    Services.obs.removeObserver(this, "on-build-contextmenu");
  },
  observe(aSubject, aTopic, aData) {
    let subject = aSubject.wrappedJSObject;
    let document = subject.menu.ownerDocument;
    let pocketEnabled = SaveToPocket.prefEnabled;

    let showSaveCurrentPageToPocket = !(
      subject.onTextInput ||
      subject.onLink ||
      subject.isContentSelected ||
      subject.onImage ||
      subject.onCanvas ||
      subject.onVideo ||
      subject.onAudio
    );
    let targetUrl = subject.onLink ? subject.linkUrl : subject.pageUrl;
    let targetURI = Services.io.newURI(targetUrl);
    let canPocket =
      pocketEnabled &&
      (targetURI.schemeIs("http") ||
        targetURI.schemeIs("https") ||
        (targetURI.schemeIs("about") && ReaderMode.getOriginalUrl(targetUrl)));

    let showSaveLinkToPocket =
      canPocket && !showSaveCurrentPageToPocket && subject.onLink;

    let menu = document.getElementById("context-pocket");
    menu.hidden = !(canPocket && showSaveCurrentPageToPocket);

    menu = document.getElementById("context-savelinktopocket");
    menu.hidden = !showSaveLinkToPocket;
  },
};

var PocketOverlay = {
  startup() {
    PocketCustomizableWidget.init();
    PocketContextMenu.init();
  },
  shutdown() {
    PocketCustomizableWidget.shutdown();
    PocketContextMenu.shutdown();
  },
};

function browserWindows() {
  return Services.wm.getEnumerator("navigator:browser");
}

var SaveToPocket = {
  init() {
    // migrate enabled pref
    if (Services.prefs.prefHasUserValue("browser.pocket.enabled")) {
      Services.prefs.setBoolPref(
        "extensions.pocket.enabled",
        Services.prefs.getBoolPref("browser.pocket.enabled")
      );
      Services.prefs.clearUserPref("browser.pocket.enabled");
    }
    // Only define the pref getter now, so we don't get notified for the
    // migrated pref above.
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "prefEnabled",
      "extensions.pocket.enabled",
      true,
      this.onPrefChange.bind(this)
    );
    if (this.prefEnabled) {
      PocketOverlay.startup();
    } else {
      // We avoid calling onPrefChange or similar here, because we don't want to
      // shut down things that haven't started up, or broadcast unnecessary messages.
      this.updateElements(false);
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    }
    AboutReaderParent.addMessageListener("Reader:OnSetup", this);
    AboutReaderParent.addMessageListener("Reader:Clicked-pocket-button", this);
  },

  observe(subject, topic, data) {
    if (topic == "browser-delayed-startup-finished") {
      // We only get here if pocket is disabled; the observer is removed when
      // we're enabled.
      this.updateElementsInWindow(subject, false);
    }
  },

  _readerButtonData: {
    id: "pocket-button",
    telemetryId: "save-to-pocket",
    label: gStrings.formatStringFromName("readerView.savetopocket.label", [
      "Pocket",
    ]),
    image: "chrome://global/skin/icons/pocket.svg",
    width: 16,
    height: 16,
  },

  onPrefChange(pref, oldValue, newValue) {
    if (!newValue) {
      AboutReaderParent.broadcastAsyncMessage("Reader:RemoveButton", {
        id: "pocket-button",
      });
      PocketOverlay.shutdown();
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    } else {
      AboutReaderParent.broadcastAsyncMessage(
        "Reader:AddButton",
        this._readerButtonData
      );
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      PocketOverlay.startup();
    }
    this.updateElements(newValue);
  },

  // Sets or removes the "pocketed" attribute on the Pocket urlbar button as
  // necessary.
  updateToolbarNodeState(browserWindow) {
    const toolbarNode = browserWindow.document.getElementById(
      "save-to-pocket-button"
    );
    if (!toolbarNode || toolbarNode.hidden) {
      return;
    }

    let browser = browserWindow.gBrowser.selectedBrowser;

    let pocketedInnerWindowID = this.innerWindowIDsByBrowser.get(browser);
    if (pocketedInnerWindowID == browser.innerWindowID) {
      // The current window in this browser is pocketed.
      toolbarNode.setAttribute("pocketed", "true");
    } else {
      // The window isn't pocketed.
      toolbarNode.removeAttribute("pocketed");
    }
  },

  // For pocketed inner windows, this maps their <browser>s to those inner
  // window IDs.  If a browser's inner window changes, then the mapped ID will
  // be out of date, meaning that the new inner window has not been pocketed.
  // If a browser goes away, then it'll be gone from this map too since it's
  // weak.  To tell whether a window has been pocketed then, look up its browser
  // in this map and compare the mapped inner window ID to the ID of the current
  // inner window.
  get innerWindowIDsByBrowser() {
    delete this.innerWindowIDsByBrowser;
    return (this.innerWindowIDsByBrowser = new WeakMap());
  },

  onLocationChange(browserWindow) {
    this.updateToolbarNodeState(browserWindow);
  },

  /**
   * Functions related to the Pocket panel UI.
   */
  onShownInToolbarPanel(panel, frame) {
    let window = panel.ownerGlobal;
    window.pktUI.setToolbarPanelFrame(frame);
    Pocket._initPanelView(window);
  },

  // If an item is saved to Pocket, we cache the browser's inner window ID,
  // so if you navigate to that tab again, we can check the ID
  // and see if we need to update the toolbar icon.
  itemSaved() {
    const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
    const browser = browserWindow.gBrowser.selectedBrowser;
    SaveToPocket.innerWindowIDsByBrowser.set(browser, browser.innerWindowID);
  },

  // If an item is removed from Pocket, we remove that browser's inner window ID,
  // so if you navigate to that tab again, we can check the ID
  // and see if we need to update the toolbar icon.
  itemDeleted() {
    const browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
    const browser = browserWindow.gBrowser.selectedBrowser;
    SaveToPocket.innerWindowIDsByBrowser.delete(browser);
  },

  updateElements(enabled) {
    // loop through windows and show/hide all our elements.
    for (let win of browserWindows()) {
      this.updateElementsInWindow(win, enabled);
    }
  },

  updateElementsInWindow(win, enabled) {
    if (enabled) {
      win.document.documentElement.removeAttribute("pocketdisabled");
    } else {
      // Hide the context menu items to ensure separator logic works.
      let savePageMenu = win.document.getElementById("context-pocket");
      let saveLinkMenu = win.document.getElementById(
        "context-savelinktopocket"
      );
      savePageMenu.hidden = saveLinkMenu.hidden = true;
      win.document.documentElement.setAttribute("pocketdisabled", "true");
    }
  },

  receiveMessage(message) {
    if (!this.prefEnabled) {
      return;
    }
    switch (message.name) {
      case "Reader:OnSetup": {
        // Tell the reader about our button.
        message.target.sendMessageToActor(
          "Reader:AddButton",
          this._readerButtonData,
          "AboutReader"
        );
        break;
      }
      case "Reader:Clicked-pocket-button": {
        // Saves the currently viewed page.
        Pocket.savePage(message.target);
        break;
      }
    }
  },
};
