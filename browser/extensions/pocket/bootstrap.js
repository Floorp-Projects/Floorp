/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ADDON_ENABLE:false, ADDON_DISABLE:false, APP_SHUTDOWN: false */

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-common/utils.js");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "AboutPocket",
                               "chrome://pocket/content/AboutPocket.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManagerPrivate",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PageActions",
                               "resource:///modules/PageActions.jsm");
ChromeUtils.defineModuleGetter(this, "Pocket",
                               "chrome://pocket/content/Pocket.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderMode",
                               "resource://gre/modules/ReaderMode.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyGetter(this, "gPocketBundle", function() {
  return Services.strings.createBundle("chrome://pocket/locale/pocket.properties");
});
XPCOMUtils.defineLazyGetter(this, "gPocketStyleURI", function() {
  return Services.io.newURI("chrome://pocket-shared/skin/pocket.css");
});

// Due to bug 1051238 frame scripts are cached forever, so we can't update them
// as a restartless add-on. The Math.random() is the work around for this.
const PROCESS_SCRIPT = "chrome://pocket/content/pocket-content-process.js?" + Math.random();

const PREF_BRANCH = "extensions.pocket.";
const PREFS = {
  enabled: true, // bug 1229937, figure out ui tour support
  api: "api.getpocket.com",
  site: "getpocket.com"
};

function setDefaultPrefs() {
  let branch = Services.prefs.getDefaultBranch(PREF_BRANCH);
  for (let [key, val] of Object.entries(PREFS)) {
    // If someone beat us to setting a default, don't overwrite it.  This can
    // happen if distribution.ini sets the default first.
    if (branch.getPrefType(key) != branch.PREF_INVALID)
      continue;
    switch (typeof val) {
      case "boolean":
        branch.setBoolPref(key, val);
        break;
      case "number":
        branch.setIntPref(key, val);
        break;
      case "string":
        branch.setCharPref(key, val);
        break;
    }
  }
}

function createElementWithAttrs(document, type, attrs) {
  let element = document.createElement(type);
  Object.keys(attrs).forEach(function(attr) {
    element.setAttribute(attr, attrs[attr]);
  });
  return element;
}


function isPocketEnabled() {
  return PocketPageAction.enabled;
}

var PocketPageAction = {
  pageAction: null,
  urlbarNode: null,

  get enabled() {
    return !!this.pageAction;
  },

  init() {
    let id = "pocket";
    this.pageAction = PageActions.actionForID(id);
    if (!this.pageAction) {
      this.pageAction = PageActions.addAction(new PageActions.Action({
        id,
        title: gPocketBundle.GetStringFromName("saveToPocketCmd.label"),
        pinnedToUrlbar: true,
        wantsIframe: true,
        urlbarIDOverride: "pocket-button-box",
        anchorIDOverride: "pocket-button",
        _insertBeforeActionID: PageActions.ACTION_ID_BOOKMARK_SEPARATOR,
        _urlbarNodeInMarkup: true,
        onBeforePlacedInWindow(window) {
          let doc = window.document;

          if (doc.getElementById("pocket-button-box")) {
            return;
          }

          let wrapper = doc.createElement("hbox");
          wrapper.id = "pocket-button-box";
          wrapper.classList.add("urlbar-icon-wrapper", "urlbar-page-action");
          let animatableBox = doc.createElement("hbox");
          animatableBox.id = "pocket-animatable-box";
          let animatableImage = doc.createElement("image");
          animatableImage.id = "pocket-animatable-image";
          animatableImage.setAttribute("role", "presentation");
          let tooltip =
            gPocketBundle.GetStringFromName("pocket-button.tooltiptext");
          animatableImage.setAttribute("tooltiptext", tooltip);
          let pocketButton = doc.createElement("image");
          pocketButton.id = "pocket-button";
          pocketButton.classList.add("urlbar-icon");
          pocketButton.setAttribute("role", "button");
          pocketButton.setAttribute("tooltiptext", tooltip);

          wrapper.appendChild(pocketButton);
          wrapper.appendChild(animatableBox);
          animatableBox.appendChild(animatableImage);
          let iconBox = doc.getElementById("page-action-buttons");
          iconBox.appendChild(wrapper);
          wrapper.hidden = true;

          wrapper.addEventListener("click", event => {
            let {BrowserPageActions} = wrapper.ownerGlobal;
            BrowserPageActions.doCommandForAction(this, event, wrapper);
          });
        },
        onIframeShowing(iframe, panel) {
          Pocket.onShownInPhotonPageActionPanel(panel, iframe);

          let doc = panel.ownerDocument;
          let urlbarNode = doc.getElementById("pocket-button-box");
          if (!urlbarNode || urlbarNode.hidden) {
            return;
          }

          BrowserUtils.setToolbarButtonHeightProperty(urlbarNode);

          PocketPageAction.urlbarNode = urlbarNode;
          PocketPageAction.urlbarNode.setAttribute("open", "true");
          if (Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled")) {
            PocketPageAction.urlbarNode.setAttribute("animate", "true");
          }

          let browser = panel.ownerGlobal.gBrowser.selectedBrowser;
          PocketPageAction.pocketedBrowser = browser;
          PocketPageAction.pocketedBrowserInnerWindowID = browser.innerWindowID;
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
      }));
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
    return this.innerWindowIDsByBrowser = new WeakMap();
  },

  // Sets or removes the "pocketed" attribute on the Pocket urlbar button as
  // necessary.
  updateUrlbarNodeState(browserWindow) {
    if (!this.pageAction) {
      return;
    }
    let {BrowserPageActions} = browserWindow;
    let urlbarNode = browserWindow.document.getElementById(
      BrowserPageActions.urlbarButtonNodeIDForActionID(this.pageAction.id)
    );
    if (!urlbarNode) {
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
      let pocketButtonBox = doc.getElementById("pocket-button-box");
      if (pocketButtonBox) {
        pocketButtonBox.remove();
      }
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
    // loop through windows and remove context menus
    // iterate through all windows and add pocket to them
    for (let win of browserWindows()) {
      let document = win.document;
      for (let id of ["context-pocket", "context-savelinktopocket"]) {
        let element = document.getElementById(id);
        if (element)
          element.remove();
      }
    }
  },
  observe(aSubject, aTopic, aData) {
    let subject = aSubject.wrappedJSObject;
    let document = subject.menu.ownerDocument;
    let pocketEnabled = isPocketEnabled();

    let showSaveCurrentPageToPocket = !(subject.onTextInput || subject.onLink ||
                                        subject.isContentSelected || subject.onImage ||
                                        subject.onCanvas || subject.onVideo || subject.onAudio);
    let targetUrl = subject.onLink ? subject.linkUrl : subject.pageUrl;
    let targetURI = Services.io.newURI(targetUrl);
    let canPocket = pocketEnabled && (targetURI.schemeIs("http") || targetURI.schemeIs("https") ||
                    (targetURI.schemeIs("about") && ReaderMode.getOriginalUrl(targetUrl)));

    let showSaveLinkToPocket = canPocket && !showSaveCurrentPageToPocket && subject.onLink;

    // create menu entries if necessary
    let menu = document.getElementById("context-pocket");
    if (!menu) {
      menu = createElementWithAttrs(document, "menuitem", {
        "id": "context-pocket",
        "label": gPocketBundle.GetStringFromName("saveToPocketCmd.label"),
        "accesskey": gPocketBundle.GetStringFromName("saveToPocketCmd.accesskey"),
        "oncommand": "Pocket.savePage(gContextMenu.browser, gContextMenu.browser.currentURI.spec, gContextMenu.browser.contentTitle);"
      });
      let sibling = document.getElementById("context-savepage");
      if (sibling.nextSibling) {
        sibling.parentNode.insertBefore(menu, sibling.nextSibling);
      } else {
        sibling.parentNode.appendChild(menu);
      }
    }
    menu.hidden = !(canPocket && showSaveCurrentPageToPocket);

    menu = document.getElementById("context-savelinktopocket");
    if (!menu) {
      menu = createElementWithAttrs(document, "menuitem", {
        "id": "context-savelinktopocket",
        "label": gPocketBundle.GetStringFromName("saveLinkToPocketCmd.label"),
        "accesskey": gPocketBundle.GetStringFromName("saveLinkToPocketCmd.accesskey"),
        "oncommand": "Pocket.savePage(gContextMenu.browser, gContextMenu.linkURL);"
      });
      let sibling = document.getElementById("context-savelink");
      if (sibling.nextSibling) {
        sibling.parentNode.insertBefore(menu, sibling.nextSibling);
      } else {
        sibling.parentNode.appendChild(menu);
      }
    }
    menu.hidden = !showSaveLinkToPocket;
  }
};

// PocketReader
// Listen for reader mode setup and add our button to the reader toolbar
var PocketReader = {
  _hidden: true,
  get hidden() {
    return this._hidden;
  },
  set hidden(hide) {
    hide = !!hide;
    if (hide === this._hidden)
      return;
    this._hidden = hide;
    this.update();
  },
  startup() {
    // Setup the listeners, update will be called when the widget is added,
    // no need to do that now.
    let mm = Services.mm;
    mm.addMessageListener("Reader:OnSetup", this);
    mm.addMessageListener("Reader:Clicked-pocket-button", this);
  },
  shutdown() {
    let mm = Services.mm;
    mm.removeMessageListener("Reader:OnSetup", this);
    mm.removeMessageListener("Reader:Clicked-pocket-button", this);
    this.hidden = true;
  },
  update() {
    if (this.hidden) {
      Services.mm.broadcastAsyncMessage("Reader:RemoveButton", { id: "pocket-button" });
    } else {
      Services.mm.broadcastAsyncMessage("Reader:AddButton",
                               { id: "pocket-button",
                                 title: gPocketBundle.GetStringFromName("pocket-button.tooltiptext"),
                                 image: "chrome://pocket/content/panels/img/pocket.svg#pocket-mark" });
    }
  },
  receiveMessage(message) {
    switch (message.name) {
      case "Reader:OnSetup": {
        // Tell the reader about our button.
        if (this.hidden)
          break;
        message.target.messageManager.
          sendAsyncMessage("Reader:AddButton", { id: "pocket-button",
                                                 title: gPocketBundle.GetStringFromName("pocket-button.tooltiptext"),
                                                 image: "chrome://pocket/content/panels/img/pocket.svg#pocket-mark"});
        break;
      }
      case "Reader:Clicked-pocket-button": {
        if (PocketPageAction.pageAction) {
          PocketPageAction.pageAction.doCommand(message.target.ownerGlobal);
        }
        break;
      }
    }
  }
};


function pktUIGetter(prop, window) {
  return {
    get() {
      // delete any getters for properties loaded from main.js so we only load main.js once
      delete window.pktUI;
      delete window.pktApi;
      delete window.pktUIMessaging;
      Services.scriptloader.loadSubScript("chrome://pocket/content/main.js", window);
      return window[prop];
    },
    configurable: true,
    enumerable: true
  };
}

var PocketOverlay = {
  startup(reason) {
    let styleSheetService = Cc["@mozilla.org/content/style-sheet-service;1"]
                              .getService(Ci.nsIStyleSheetService);
    this._sheetType = styleSheetService.AUTHOR_SHEET;
    this._cachedSheet = styleSheetService.preloadSheet(gPocketStyleURI,
                                                       this._sheetType);
    Services.ppmm.loadProcessScript(PROCESS_SCRIPT, true);
    Services.obs.addObserver(this, "browser-delayed-startup-finished");
    PocketReader.startup();
    PocketPageAction.init();
    PocketContextMenu.init();
    for (let win of browserWindows()) {
      this.onWindowOpened(win);
    }
  },
  shutdown(reason) {
    Services.ppmm.broadcastAsyncMessage("PocketShuttingDown");
    Services.obs.removeObserver(this, "browser-delayed-startup-finished");
    // Although the ppmm loads the scripts into the chrome process as well,
    // we need to manually unregister here anyway to ensure these aren't part
    // of the chrome process and avoid errors.
    AboutPocket.aboutSaved.unregister();
    AboutPocket.aboutSignup.unregister();

    PocketPageAction.shutdown();

    for (let window of browserWindows()) {
      for (let id of ["appMenu-library-pocket-button"]) {
        let element = window.document.getElementById(id) ||
                      window.gNavToolbox.palette.querySelector("#" + id);
        if (element)
          element.remove();
      }
      this.removeStyles(window);
      // remove script getters/objects
      delete window.Pocket;
      delete window.pktApi;
      delete window.pktUI;
      delete window.pktUIMessaging;
    }

    PocketContextMenu.shutdown();
    PocketReader.shutdown();
  },
  observe(subject, topic, detail) {
    if (topic == "browser-delayed-startup-finished") {
      this.onWindowOpened(subject);
    }
  },
  onWindowOpened(window) {
    if (window.hasOwnProperty("pktUI"))
      return;
    this.setWindowScripts(window);
    this.addStyles(window);
    this.updateWindow(window);
  },
  setWindowScripts(window) {
    ChromeUtils.defineModuleGetter(window, "Pocket",
                                   "chrome://pocket/content/Pocket.jsm");
    // Can't use XPCOMUtils for these because the scripts try to define the variables
    // on window, and so the defineProperty inside defineLazyGetter fails.
    Object.defineProperty(window, "pktApi", pktUIGetter("pktApi", window));
    Object.defineProperty(window, "pktUI", pktUIGetter("pktUI", window));
    Object.defineProperty(window, "pktUIMessaging", pktUIGetter("pktUIMessaging", window));
  },
  // called for each window as it is opened
  updateWindow(window) {
    // insert our three menu items
    let document = window.document;
    let hidden = !isPocketEnabled();

    // Add to library panel
    let sib = document.getElementById("appMenu-library-history-button");
    if (sib && !document.getElementById("appMenu-library-pocket-button")) {
      let menu = createElementWithAttrs(document, "toolbarbutton", {
        "id": "appMenu-library-pocket-button",
        "label": gPocketBundle.GetStringFromName("pocketMenuitem.label"),
        "class": "subviewbutton subviewbutton-iconic",
        "oncommand": "Pocket.openList(event)",
        "hidden": hidden
      });
      sib.parentNode.insertBefore(menu, sib);
    }

    // enable or disable reader button
    PocketReader.hidden = hidden;
  },

  addStyles(win) {
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    utils.addSheet(this._cachedSheet, this._sheetType);
  },

  removeStyles(win) {
    let utils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    utils.removeSheet(gPocketStyleURI, this._sheetType);
  }

};

// use enabled pref as a way for tests (e.g. test_contextmenu.html) to disable
// the addon when running.
function prefObserver(aSubject, aTopic, aData) {
  let enabled = Services.prefs.getBoolPref("extensions.pocket.enabled");
  if (enabled)
    PocketOverlay.startup(ADDON_ENABLE);
  else
    PocketOverlay.shutdown(ADDON_DISABLE);
}

function startup(data, reason) {
  if (AddonManagerPrivate.addonIsActive("isreaditlater@ideashower.com"))
    return;

  setDefaultPrefs();
  // migrate enabled pref
  if (Services.prefs.prefHasUserValue("browser.pocket.enabled")) {
    Services.prefs.setBoolPref("extensions.pocket.enabled", Services.prefs.getBoolPref("browser.pocket.enabled"));
    Services.prefs.clearUserPref("browser.pocket.enabled");
  }
  // watch pref change and enable/disable if necessary
  Services.prefs.addObserver("extensions.pocket.enabled", prefObserver);
  if (!Services.prefs.getBoolPref("extensions.pocket.enabled"))
    return;
  PocketOverlay.startup(reason);
}

function shutdown(data, reason) {
  // For speed sake, we should only do a shutdown if we're being disabled.
  // On an app shutdown, just let it fade away...
  if (reason != APP_SHUTDOWN) {
    Services.prefs.removeObserver("extensions.pocket.enabled", prefObserver);
    PocketOverlay.shutdown(reason);
  }
}

function install() {
}

function uninstall() {
}

function* browserWindows() {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    yield windows.getNext();
  }
}
