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
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PageActions",
  "resource:///modules/PageActions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Pocket",
  "chrome://pocket/content/Pocket.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm"
);

var EXPORTED_SYMBOLS = ["SaveToPocket"];

var PocketPageAction = {
  pageAction: null,
  urlbarNode: null,

  init() {
    let id = "pocket";
    this.pageAction = PageActions.actionForID(id);
    if (!this.pageAction) {
      this.pageAction = PageActions.addAction(
        new PageActions.Action({
          id,
          title: "pocket-title",
          pinnedToUrlbar: true,
          wantsIframe: true,
          urlbarIDOverride: "pocket-button",
          anchorIDOverride: "pocket-button",
          _insertBeforeActionID: PageActions.ACTION_ID_PIN_TAB,
          _urlbarNodeInMarkup: true,
          onBeforePlacedInWindow(window) {
            let action = PageActions.actionForID("pocket");
            window.BrowserPageActions.takeActionTitleFromPanel(action);
          },
          onIframeShowing(iframe, panel) {
            Pocket.onShownInPhotonPageActionPanel(panel, iframe);

            let doc = panel.ownerDocument;
            let urlbarNode = doc.getElementById("pocket-button");
            if (!urlbarNode) {
              return;
            }

            BrowserUtils.setToolbarButtonHeightProperty(urlbarNode);

            PocketPageAction.urlbarNode = urlbarNode;
            PocketPageAction.urlbarNode.setAttribute("open", "true");

            let browser = panel.ownerGlobal.gBrowser.selectedBrowser;
            PocketPageAction.pocketedBrowser = browser;
            PocketPageAction.pocketedBrowserInnerWindowID =
              browser.innerWindowID;
          },
          onIframeHidden(iframe, panel) {
            if (iframe.getAttribute("itemAdded") == "true") {
              iframe.ownerGlobal.LibraryUI.triggerLibraryAnimation("pocket");
            }

            if (!PocketPageAction.urlbarNode) {
              return;
            }
            PocketPageAction.urlbarNode.removeAttribute("animate");
            PocketPageAction.urlbarNode.removeAttribute("open");
            delete PocketPageAction.urlbarNode;

            if (iframe.getAttribute("itemAdded") == "true") {
              PocketPageAction.innerWindowIDsByBrowser.set(
                PocketPageAction.pocketedBrowser,
                PocketPageAction.pocketedBrowserInnerWindowID
              );
            } else {
              PocketPageAction.innerWindowIDsByBrowser.delete(
                PocketPageAction.pocketedBrowser
              );
            }
            PocketPageAction.updateUrlbarNodeState(panel.ownerGlobal);
            delete PocketPageAction.pocketedBrowser;
            delete PocketPageAction.pocketedBrowserInnerWindowID;
          },
          onLocationChange(browserWindow) {
            PocketPageAction.updateUrlbarNodeState(browserWindow);
          },
        })
      );
    }
    Pocket.pageAction = this.pageAction;
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

  // Sets or removes the "pocketed" attribute on the Pocket urlbar button as
  // necessary.
  updateUrlbarNodeState(browserWindow) {
    if (!this.pageAction) {
      return;
    }
    let { BrowserPageActions } = browserWindow;
    let urlbarNode = browserWindow.document.getElementById(
      BrowserPageActions.urlbarButtonNodeIDForActionID(this.pageAction.id)
    );
    if (!urlbarNode || urlbarNode.hidden) {
      return;
    }
    let browser = browserWindow.gBrowser.selectedBrowser;
    let pocketedInnerWindowID = this.innerWindowIDsByBrowser.get(browser);
    if (pocketedInnerWindowID == browser.innerWindowID) {
      // The current window in this browser is pocketed.
      urlbarNode.setAttribute("pocketed", "true");
    } else {
      // The window isn't pocketed.
      urlbarNode.removeAttribute("pocketed");
    }
  },

  shutdown() {
    if (!this.pageAction) {
      return;
    }

    for (let win of browserWindows()) {
      let doc = win.document;
      let pocketButton = doc.getElementById("pocket-button");
      pocketButton.setAttribute("hidden", "true");
    }

    this.pageAction.remove();
    this.pageAction = null;
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
    PocketPageAction.init();
    PocketContextMenu.init();
  },
  shutdown() {
    PocketPageAction.shutdown();
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
    Services.mm.addMessageListener("Reader:OnSetup", this);
    Services.mm.addMessageListener("Reader:Clicked-pocket-button", this);
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
    image: "chrome://pocket/content/panels/img/pocket-outline.svg",
    width: 20,
    height: 20,
  },

  onPrefChange(pref, oldValue, newValue) {
    if (!newValue) {
      Services.mm.broadcastAsyncMessage("Reader:RemoveButton", {
        id: "pocket-button",
      });
      PocketOverlay.shutdown();
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
    } else {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      PocketOverlay.startup();
      // The title for the button is extracted from browser.xhtml where it comes from a DTD.
      // If we don't have this, there's also no possibility of there being a reader
      // mode tab already loaded. We'll get an Reader:OnSetup message when that happens.
      if (this._readerButtonData.title) {
        Services.mm.broadcastAsyncMessage(
          "Reader:AddButton",
          this._readerButtonData
        );
      }
    }
    this.updateElements(newValue);
  },

  updateElements(enabled) {
    // loop through windows and show/hide all our elements.
    for (let win of browserWindows()) {
      this.updateElementsInWindow(win, enabled);
    }
  },

  updateElementsInWindow(win, enabled) {
    let elementIds = [
      "context-pocket",
      "context-savelinktopocket",
      "appMenu-library-pocket-button",
    ];
    let document = win.document;
    for (let id of elementIds) {
      document.getElementById(id).hidden = !enabled;
    }
  },

  receiveMessage(message) {
    if (!this.prefEnabled) {
      return;
    }
    switch (message.name) {
      case "Reader:OnSetup": {
        // We must have a browser window; get the tooltip for the button if we don't
        // have it already.
        if (!this._readerButtonData.title) {
          let doc = message.target.ownerDocument;
          let button = doc.getElementById("pocket-button");
          this._readerButtonData.title = button.getAttribute("tooltiptext");
        }
        // Tell the reader about our button.
        message.target.messageManager.sendAsyncMessage(
          "Reader:AddButton",
          this._readerButtonData
        );
        break;
      }
      case "Reader:Clicked-pocket-button": {
        PocketPageAction.pageAction.doCommand(message.target.ownerGlobal);
        break;
      }
    }
  },
};
