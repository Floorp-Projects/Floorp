/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
ChromeUtils.defineModuleGetter(
  lazy,
  "Pocket",
  "chrome://pocket/content/Pocket.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "AboutReaderParent",
  "resource:///actors/AboutReaderParent.jsm"
);

var EXPORTED_SYMBOLS = ["SaveToPocket"];

const gStrings = Services.strings.createBundle(
  "chrome://global/locale/aboutReader.properties"
);
var PocketCustomizableWidget = {
  init() {
    lazy.CustomizableUI.createWidget({
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
        frame.setAttribute("remote", true);
        frame.setAttribute("autocompletepopup", "PopupAutoComplete");
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
    lazy.CustomizableUI.destroyWidget("save-to-pocket-button");
  },
};

var PocketOverlay = {
  startup() {
    PocketCustomizableWidget.init();
  },
  shutdown() {
    PocketCustomizableWidget.shutdown();
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
    lazy.AboutReaderParent.addMessageListener("Reader:OnSetup", this);
    lazy.AboutReaderParent.addMessageListener(
      "Reader:Clicked-pocket-button",
      this
    );
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
  },

  onPrefChange(pref, oldValue, newValue) {
    if (!newValue) {
      lazy.AboutReaderParent.broadcastAsyncMessage("Reader:RemoveButton", {
        id: "pocket-button",
      });
      PocketOverlay.shutdown();
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    } else {
      lazy.AboutReaderParent.broadcastAsyncMessage(
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
    lazy.Pocket._initPanelView(window);
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
        let pocketPanel = message.target.ownerDocument.querySelector(
          "#customizationui-widget-panel"
        );
        if (pocketPanel?.getAttribute("panelopen")) {
          pocketPanel.hidePopup();
        } else {
          // Saves the currently viewed page.
          lazy.Pocket.savePage(message.target);
        }
        break;
      }
    }
  },
};
