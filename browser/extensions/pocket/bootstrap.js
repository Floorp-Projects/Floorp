/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global ADDON_ENABLE:false, ADDON_DISABLE:false, APP_SHUTDOWN: false */

const {classes: Cc, interfaces: Ci, utils: Cu, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow",
                                  "resource:///modules/RecentWindow.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
                                  "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Pocket",
                                  "chrome://pocket/content/Pocket.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutPocket",
                                  "chrome://pocket/content/AboutPocket.jsm");
XPCOMUtils.defineLazyGetter(this, "gPocketBundle", function() {
  return Services.strings.createBundle("chrome://pocket/locale/pocket.properties");
});
XPCOMUtils.defineLazyGetter(this, "gPocketStyleURI", function() {
  return Services.io.newURI("chrome://pocket/skin/pocket.css");
});

// Due to bug 1051238 frame scripts are cached forever, so we can't update them
// as a restartless add-on. The Math.random() is the work around for this.
const PROCESS_SCRIPT = "chrome://pocket/content/pocket-content-process.js?" + Math.random();

const PREF_BRANCH = "extensions.pocket.";
const PREFS = {
  enabled: true, // bug 1229937, figure out ui tour support
  api: "api.getpocket.com",
  site: "getpocket.com",
  oAuthConsumerKey: "40249-e88c401e1b1f2242d9e441c4"
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
  })
  return element;
}

function CreatePocketWidget(reason) {
  let id = "pocket-button"
  let widget = CustomizableUI.getWidget(id);
  // The widget is only null if we've created then destroyed the widget.
  // Once we've actually called createWidget the provider will be set to
  // PROVIDER_API.
  if (widget && widget.provider == CustomizableUI.PROVIDER_API)
    return;
  // if upgrading from builtin version and the button was placed in ui,
  // seenWidget will not be null
  let seenWidget = CustomizableUI.getPlacementOfWidget("pocket-button", false, true);
  let pocketButton = {
    id: "pocket-button",
    defaultArea: CustomizableUI.AREA_NAVBAR,
    introducedInVersion: "pref",
    type: "view",
    tabSpecific: true,
    viewId: "PanelUI-pocketView",
    label: gPocketBundle.GetStringFromName("pocket-button.label"),
    tooltiptext: gPocketBundle.GetStringFromName("pocket-button.tooltiptext"),
    // Use forwarding functions here to avoid loading Pocket.jsm on startup:
    onViewShowing() {
      return Pocket.onPanelViewShowing.apply(this, arguments);
    },
    onViewHiding() {
      return Pocket.onPanelViewHiding.apply(this, arguments);
    },
    onBeforeCreated(doc) {
      // Bug 1223127,CUI should make this easier to do.
      if (doc.getElementById("PanelUI-pocketView"))
        return;
      let view = doc.createElement("panelview");
      view.id = "PanelUI-pocketView";
      let panel = doc.createElement("vbox");
      panel.setAttribute("class", "panel-subview-body");
      view.appendChild(panel);
      doc.getElementById("PanelUI-multiView").appendChild(view);
    }
  };

  CustomizableUI.createWidget(pocketButton);
  CustomizableUI.addListener(pocketButton);
  // placed is null if location is palette
  let placed = CustomizableUI.getPlacementOfWidget("pocket-button");

  // a first time install will always have placed the button somewhere, and will
  // not have a placement prior to creating the widget. Thus, !seenWidget &&
  // placed.
  if (reason == ADDON_ENABLE && !seenWidget && placed) {
    // initially place the button after the bookmarks button if it is in the UI
    let widgets = CustomizableUI.getWidgetIdsInArea(CustomizableUI.AREA_NAVBAR);
    let bmbtn = widgets.indexOf("bookmarks-menu-button");
    if (bmbtn > -1) {
      CustomizableUI.moveWidgetWithinArea("pocket-button", bmbtn + 1);
    }
  }
}

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
    for (let win of CustomizableUI.windows) {
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
    let pocketEnabled = CustomizableUI.getPlacementOfWidget("pocket-button");

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
}

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
        let doc = message.target.ownerDocument;
        let pocketWidget = doc.getElementById("pocket-button");
        let placement = CustomizableUI.getPlacementOfWidget("pocket-button");
        if (placement) {
          if (placement.area == CustomizableUI.AREA_PANEL) {
            doc.defaultView.PanelUI.show().then(function() {
              // The DOM node might not exist yet if the panel wasn't opened before.
              pocketWidget = doc.getElementById("pocket-button");
              pocketWidget.doCommand();
            });
          } else {
            pocketWidget.doCommand();
          }
        }
        break;
      }
    }
  }
}


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
    PocketReader.startup();
    CustomizableUI.addListener(this);
    CreatePocketWidget(reason);
    PocketContextMenu.init();

    for (let win of CustomizableUI.windows) {
      this.onWindowOpened(win);
    }
  },
  shutdown(reason) {
    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);
    ppmm.broadcastAsyncMessage("PocketShuttingDown");
    // Although the ppmm loads the scripts into the chrome process as well,
    // we need to manually unregister here anyway to ensure these aren't part
    // of the chrome process and avoid errors.
    AboutPocket.aboutSaved.unregister();
    AboutPocket.aboutSignup.unregister();

    CustomizableUI.removeListener(this);
    for (let window of CustomizableUI.windows) {
      for (let id of ["panelMenu_pocket", "menu_pocket", "BMB_pocket",
                      "panelMenu_pocketSeparator", "menu_pocketSeparator",
                      "BMB_pocketSeparator", "appMenu-library-pocket-button"]) {
        let element = window.document.getElementById(id);
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
    CustomizableUI.destroyWidget("pocket-button");
    PocketContextMenu.shutdown();
    PocketReader.shutdown();
  },
  onWindowOpened(window) {
    if (window.hasOwnProperty("pktUI"))
      return;
    this.setWindowScripts(window);
    this.addStyles(window);
    this.updateWindow(window);
  },
  setWindowScripts(window) {
    XPCOMUtils.defineLazyModuleGetter(window, "Pocket",
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
    let hidden = !CustomizableUI.getPlacementOfWidget("pocket-button");

    // add to bookmarksMenu
    let sib = document.getElementById("menu_bookmarkThisPage");
    if (sib && !document.getElementById("menu_pocket")) {
      let menu = createElementWithAttrs(document, "menuitem", {
        "id": "menu_pocket",
        "label": gPocketBundle.GetStringFromName("pocketMenuitem.label"),
        "class": "menuitem-iconic", // OSX only
        "oncommand": "openUILink(Pocket.listURL, event);",
        "hidden": hidden
      });
      let sep = createElementWithAttrs(document, "menuseparator", {
        "id": "menu_pocketSeparator",
        "hidden": hidden
      });
      sib.parentNode.insertBefore(menu, sib);
      sib.parentNode.insertBefore(sep, sib);
    }

    // add to bookmarks-menu-button
    sib = document.getElementById("BMB_bookmarksToolbar");
    if (sib && !document.getElementById("BMB_pocket")) {
      let menu = createElementWithAttrs(document, "menuitem", {
        "id": "BMB_pocket",
        "label": gPocketBundle.GetStringFromName("pocketMenuitem.label"),
        "class": "menuitem-iconic bookmark-item subviewbutton",
        "oncommand": "openUILink(Pocket.listURL, event);",
        "hidden": hidden
      });
      let sep = createElementWithAttrs(document, "menuseparator", {
        "id": "BMB_pocketSeparator",
        "hidden": hidden
      });
      sib.parentNode.insertBefore(menu, sib);
      sib.parentNode.insertBefore(sep, sib);
    }

    // add to PanelUI-bookmarks
    sib = document.getElementById("panelMenuBookmarkThisPage");
    if (sib && !document.getElementById("panelMenu_pocket")) {
      let menu = createElementWithAttrs(document, "toolbarbutton", {
        "id": "panelMenu_pocket",
        "label": gPocketBundle.GetStringFromName("pocketMenuitem.label"),
        "class": "subviewbutton cui-withicon",
        "oncommand": "openUILink(Pocket.listURL, event);",
        "hidden": hidden
      });
      let sep = createElementWithAttrs(document, "toolbarseparator", {
        "id": "panelMenu_pocketSeparator",
        "hidden": hidden
      });
      // nextSibling is no-id toolbarseparator
      // insert separator first then button
      sib = sib.nextSibling;
      sib.parentNode.insertBefore(sep, sib);
      sib.parentNode.insertBefore(menu, sib);
    }

    // Add to library panel
    sib = document.getElementById("appMenu-library-history-button");
    if (sib && !document.getElementById("appMenu-library-pocket-button")) {
      let menu = createElementWithAttrs(document, "toolbarbutton", {
        "id": "appMenu-library-pocket-button",
        "label": gPocketBundle.GetStringFromName("pocketMenuitem.label"),
        "class": "subviewbutton subviewbutton-iconic",
        "oncommand": "openUILink(Pocket.listURL, event);",
        "hidden": hidden
      });
      sib.parentNode.insertBefore(menu, sib);
    }
  },
  onWidgetAfterDOMChange(aWidgetNode) {
    if (aWidgetNode.id != "pocket-button") {
      return;
    }
    let doc = aWidgetNode.ownerDocument;
    let hidden = !CustomizableUI.getPlacementOfWidget("pocket-button");
    let elementIds = [
      "panelMenu_pocket",
      "menu_pocket",
      "BMB_pocket",
      "appMenu-library-pocket-button",
    ];
    for (let elementId of elementIds) {
      let element = doc.getElementById(elementId);
      if (element) {
        element.hidden = hidden;
        let sep = doc.getElementById(elementId + "Separator");
        if (sep) {
          sep.hidden = hidden;
        }
      }
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

}

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
