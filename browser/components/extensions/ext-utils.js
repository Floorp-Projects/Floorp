/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

/* globals TabBase, WindowBase, TabTrackerBase, WindowTrackerBase, TabManagerBase, WindowManagerBase */
Cu.import("resource://gre/modules/ExtensionTabs.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

const POPUP_LOAD_TIMEOUT_MS = 200;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var {
  DefaultWeakMap,
  ExtensionError,
  SingletonEventManager,
  defineLazyGetter,
  promiseEvent,
} = ExtensionUtils;

let tabTracker;
let windowTracker;

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.

global.makeWidgetId = id => {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
};

function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.state == "open") {
      resolve();
    } else {
      popup.addEventListener("popupshown", function(event) {
        resolve();
      }, {once: true});
    }
  });
}

XPCOMUtils.defineLazyGetter(this, "popupStylesheets", () => {
  let stylesheets = ["chrome://browser/content/extension.css"];

  if (AppConstants.platform === "macosx") {
    stylesheets.push("chrome://browser/content/extension-mac.css");
  }
  return stylesheets;
});

XPCOMUtils.defineLazyGetter(this, "standaloneStylesheets", () => {
  let stylesheets = [];

  if (AppConstants.platform === "macosx") {
    stylesheets.push("chrome://browser/content/extension-mac-panel.css");
  }
  if (AppConstants.platform === "win") {
    stylesheets.push("chrome://browser/content/extension-win-panel.css");
  }
  return stylesheets;
});

class BasePopup {
  constructor(extension, viewNode, popupURL, browserStyle, fixedWidth = false) {
    this.extension = extension;
    this.popupURL = popupURL;
    this.viewNode = viewNode;
    this.browserStyle = browserStyle;
    this.window = viewNode.ownerGlobal;
    this.destroyed = false;
    this.fixedWidth = fixedWidth;

    extension.callOnClose(this);

    this.contentReady = new Promise(resolve => {
      this._resolveContentReady = resolve;
    });

    this.viewNode.addEventListener(this.DESTROY_EVENT, this);

    let doc = viewNode.ownerDocument;
    let arrowContent = doc.getAnonymousElementByAttribute(this.panel, "class", "panel-arrowcontent");
    this.borderColor = doc.defaultView.getComputedStyle(arrowContent).borderTopColor;

    this.browser = null;
    this.browserLoaded = new Promise((resolve, reject) => {
      this.browserLoadedDeferred = {resolve, reject};
    });
    this.browserReady = this.createBrowser(viewNode, popupURL);

    BasePopup.instances.get(this.window).set(extension, this);
  }

  static for(extension, window) {
    return BasePopup.instances.get(window).get(extension);
  }

  close() {
    this.closePopup();
  }

  destroy() {
    this.extension.forgetOnClose(this);

    this.destroyed = true;
    this.browserLoadedDeferred.reject(new Error("Popup destroyed"));
    return this.browserReady.then(() => {
      this.destroyBrowser(this.browser, true);
      this.browser.remove();

      if (this.viewNode) {
        this.viewNode.removeEventListener(this.DESTROY_EVENT, this);
        this.viewNode.style.maxHeight = "";
      }

      if (this.panel) {
        this.panel.style.removeProperty("--arrowpanel-background");
        this.panel.style.removeProperty("--panel-arrow-image-vertical");
      }

      BasePopup.instances.get(this.window).delete(this.extension);

      this.browser = null;
      this.viewNode = null;
    });
  }

  destroyBrowser(browser, finalize = false) {
    let mm = browser.messageManager;
    // If the browser has already been removed from the document, because the
    // popup was closed externally, there will be no message manager here, so
    // just replace our receiveMessage method with a stub.
    if (mm) {
      mm.removeMessageListener("DOMTitleChanged", this);
      mm.removeMessageListener("Extension:BrowserBackgroundChanged", this);
      mm.removeMessageListener("Extension:BrowserContentLoaded", this);
      mm.removeMessageListener("Extension:BrowserResized", this);
      mm.removeMessageListener("Extension:DOMWindowClose", this);
    } else if (finalize) {
      this.receiveMessage = () => {};
    }
  }

  // Returns the name of the event fired on `viewNode` when the popup is being
  // destroyed. This must be implemented by every subclass.
  get DESTROY_EVENT() {
    throw new Error("Not implemented");
  }

  get STYLESHEETS() {
    let sheets = [];

    if (this.browserStyle) {
      sheets.push(...popupStylesheets);
    }
    if (!this.fixedWidth) {
      sheets.push(...standaloneStylesheets);
    }

    return sheets;
  }

  get panel() {
    let panel = this.viewNode;
    while (panel && panel.localName != "panel") {
      panel = panel.parentNode;
    }
    return panel;
  }

  receiveMessage({name, data}) {
    switch (name) {
      case "DOMTitleChanged":
        this.viewNode.setAttribute("aria-label", this.browser.contentTitle);
        break;

      case "Extension:BrowserBackgroundChanged":
        this.setBackground(data.background);
        break;

      case "Extension:BrowserContentLoaded":
        this.browserLoadedDeferred.resolve();
        break;

      case "Extension:BrowserResized":
        this._resolveContentReady();
        if (this.ignoreResizes) {
          this.dimensions = data;
        } else {
          this.resizeBrowser(data);
        }
        break;

      case "Extension:DOMWindowClose":
        this.closePopup();
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case this.DESTROY_EVENT:
        if (!this.destroyed) {
          this.destroy();
        }
        break;
    }
  }

  createBrowser(viewNode, popupURL = null) {
    let document = viewNode.ownerDocument;
    let browser = document.createElementNS(XUL_NS, "browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("transparent", "true");
    browser.setAttribute("class", "webextension-popup-browser");
    browser.setAttribute("webextension-view-type", "popup");
    browser.setAttribute("tooltip", "aHTMLTooltip");

    if (this.extension.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
    }

    // We only need flex sizing for the sake of the slide-in sub-views of the
    // main menu panel, so that the browser occupies the full width of the view,
    // and also takes up any extra height that's available to it.
    browser.setAttribute("flex", "1");

    // Note: When using noautohide panels, the popup manager will add width and
    // height attributes to the panel, breaking our resize code, if the browser
    // starts out smaller than 30px by 10px. This isn't an issue now, but it
    // will be if and when we popup debugging.

    this.browser = browser;

    let readyPromise;
    if (this.extension.remote) {
      readyPromise = promiseEvent(browser, "XULFrameLoaderCreated");
    } else {
      readyPromise = promiseEvent(browser, "load");
    }

    viewNode.appendChild(browser);

    extensions.emit("extension-browser-inserted", browser);

    let setupBrowser = browser => {
      let mm = browser.messageManager;
      mm.addMessageListener("DOMTitleChanged", this);
      mm.addMessageListener("Extension:BrowserBackgroundChanged", this);
      mm.addMessageListener("Extension:BrowserContentLoaded", this);
      mm.addMessageListener("Extension:BrowserResized", this);
      mm.addMessageListener("Extension:DOMWindowClose", this, true);
      return browser;
    };

    if (!popupURL) {
      // For remote browsers, we can't do any setup until the frame loader is
      // created. Non-remote browsers get a message manager immediately, so
      // there's no need to wait for the load event.
      if (this.extension.remote) {
        return readyPromise.then(() => setupBrowser(browser));
      }
      return setupBrowser(browser);
    }

    return readyPromise.then(() => {
      setupBrowser(browser);
      let mm = browser.messageManager;

      mm.loadFrameScript(
        "chrome://extensions/content/ext-browser-content.js", false);

      mm.sendAsyncMessage("Extension:InitBrowser", {
        allowScriptsToClose: true,
        fixedWidth: this.fixedWidth,
        maxWidth: 800,
        maxHeight: 600,
        stylesheets: this.STYLESHEETS,
      });

      browser.loadURI(popupURL);
    });
  }

  resizeBrowser({width, height, detail}) {
    if (this.fixedWidth) {
      // Figure out how much extra space we have on the side of the panel
      // opposite the arrow.
      let side = this.panel.getAttribute("side") == "top" ? "bottom" : "top";
      let maxHeight = this.viewHeight + this.extraHeight[side];

      height = Math.min(height, maxHeight);
      this.browser.style.height = `${height}px`;

      // Set a maximum height on the <panelview> element to our preferred
      // maximum height, so that the PanelUI resizing code can make an accurate
      // calculation. If we don't do this, the flex sizing logic will prevent us
      // from ever reporting a preferred size smaller than the height currently
      // available to us in the panel.
      height = Math.max(height, this.viewHeight);
      this.viewNode.style.maxHeight = `${height}px`;
    } else {
      this.browser.style.width = `${width}px`;
      this.browser.style.minWidth = `${width}px`;
      this.browser.style.height = `${height}px`;
      this.browser.style.minHeight = `${height}px`;
    }

    let event = new this.window.CustomEvent("WebExtPopupResized", {detail});
    this.browser.dispatchEvent(event);
  }

  setBackground(background) {
    let panelBackground = "";
    let panelArrow = "";

    if (background) {
      let borderColor = this.borderColor || background;

      panelBackground = background;
      panelArrow = `url("data:image/svg+xml,${encodeURIComponent(`<?xml version="1.0" encoding="UTF-8"?>
        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="10">
          <path d="M 0,10 L 10,0 20,10 z" fill="${borderColor}"/>
          <path d="M 1,10 L 10,1 19,10 z" fill="${background}"/>
        </svg>
      `)}")`;
    }

    this.panel.style.setProperty("--arrowpanel-background", panelBackground);
    this.panel.style.setProperty("--panel-arrow-image-vertical", panelArrow);
    this.background = background;
  }
}

/**
 * A map of active popups for a given browser window.
 *
 * WeakMap[window -> WeakMap[Extension -> BasePopup]]
 */
BasePopup.instances = new DefaultWeakMap(() => new WeakMap());

class PanelPopup extends BasePopup {
  constructor(extension, imageNode, popupURL, browserStyle) {
    let document = imageNode.ownerDocument;

    let panel = document.createElement("panel");
    panel.setAttribute("id", makeWidgetId(extension.id) + "-panel");
    panel.setAttribute("class", "browser-extension-panel");
    panel.setAttribute("tabspecific", "true");
    panel.setAttribute("type", "arrow");
    panel.setAttribute("role", "group");

    document.getElementById("mainPopupSet").appendChild(panel);

    super(extension, panel, popupURL, browserStyle);

    this.contentReady.then(() => {
      panel.openPopup(imageNode, "bottomcenter topright", 0, 0, false, false);

      let event = new this.window.CustomEvent("WebExtPopupLoaded", {
        bubbles: true,
        detail: {extension},
      });
      this.browser.dispatchEvent(event);
    });
  }

  get DESTROY_EVENT() {
    return "popuphidden";
  }

  destroy() {
    super.destroy();
    this.viewNode.remove();
    this.viewNode = null;
  }

  closePopup() {
    promisePopupShown(this.viewNode).then(() => {
      // Make sure we're not already destroyed, or removed from the DOM.
      if (this.viewNode && this.viewNode.hidePopup) {
        this.viewNode.hidePopup();
      }
    });
  }
}

class ViewPopup extends BasePopup {
  constructor(extension, window, popupURL, browserStyle, fixedWidth) {
    let document = window.document;

    // Create a temporary panel to hold the browser while it pre-loads its
    // content. This panel will never be shown, but the browser's docShell will
    // be swapped with the browser in the real panel when it's ready.
    let panel = document.createElement("panel");
    panel.setAttribute("type", "arrow");
    document.getElementById("mainPopupSet").appendChild(panel);

    super(extension, panel, popupURL, browserStyle, fixedWidth);

    this.ignoreResizes = true;

    this.attached = false;
    this.shown = false;
    this.tempPanel = panel;

    this.browser.classList.add("webextension-preload-browser");
  }

  /**
   * Attaches the pre-loaded browser to the given view node, and reserves a
   * promise which resolves when the browser is ready.
   *
   * @param {Element} viewNode
   *        The node to attach the browser to.
   * @returns {Promise<boolean>}
   *        Resolves when the browser is ready. Resolves to `false` if the
   *        browser was destroyed before it was fully loaded, and the popup
   *        should be closed, or `true` otherwise.
   */
  attach(viewNode) {
    return Task.spawn(function* () {
      this.viewNode = viewNode;
      this.viewNode.addEventListener(this.DESTROY_EVENT, this);

      // Wait until the browser element is fully initialized, and give it at least
      // a short grace period to finish loading its initial content, if necessary.
      //
      // In practice, the browser that was created by the mousdown handler should
      // nearly always be ready by this point.
      yield Promise.all([
        this.browserReady,
        Promise.race([
          // This promise may be rejected if the popup calls window.close()
          // before it has fully loaded.
          this.browserLoaded.catch(() => {}),
          new Promise(resolve => setTimeout(resolve, POPUP_LOAD_TIMEOUT_MS)),
        ]),
      ]);

      if (!this.destroyed && !this.panel) {
        this.destroy();
      }

      if (this.destroyed) {
        CustomizableUI.hidePanelForNode(viewNode);
        return false;
      }

      this.attached = true;


      // Store the initial height of the view, so that we never resize menu panel
      // sub-views smaller than the initial height of the menu.
      this.viewHeight = this.viewNode.boxObject.height;

      // Calculate the extra height available on the screen above and below the
      // menu panel. Use that to calculate the how much the sub-view may grow.
      let popupRect = this.panel.getBoundingClientRect();

      this.setBackground(this.background);

      let win = this.window;
      let popupBottom = win.mozInnerScreenY + popupRect.bottom;
      let popupTop = win.mozInnerScreenY + popupRect.top;

      let screenBottom = win.screen.availTop + win.screen.availHeight;
      this.extraHeight = {
        bottom: Math.max(0, screenBottom - popupBottom),
        top:  Math.max(0, popupTop - win.screen.availTop),
      };

      // Create a new browser in the real popup.
      let browser = this.browser;
      yield this.createBrowser(this.viewNode);

      this.ignoreResizes = false;

      this.browser.swapDocShells(browser);
      this.destroyBrowser(browser);

      if (this.dimensions && !this.fixedWidth) {
        this.resizeBrowser(this.dimensions);
      }

      this.tempPanel.remove();
      this.tempPanel = null;

      this.shown = true;

      if (this.destroyed) {
        this.closePopup();
        this.destroy();
        return false;
      }

      let event = new this.window.CustomEvent("WebExtPopupLoaded", {
        bubbles: true,
        detail: {extension: this.extension},
      });
      this.browser.dispatchEvent(event);

      return true;
    }.bind(this));
  }

  destroy() {
    return super.destroy().then(() => {
      if (this.tempPanel) {
        this.tempPanel.remove();
        this.tempPanel = null;
      }
    });
  }

  get DESTROY_EVENT() {
    return "ViewHiding";
  }

  closePopup() {
    if (this.shown) {
      CustomizableUI.hidePanelForNode(this.viewNode);
    } else if (this.attached) {
      this.destroyed = true;
    } else {
      this.destroy();
    }
  }
}

Object.assign(global, {PanelPopup, ViewPopup});

// Manages tab-specific context data, and dispatching tab select events
// across all windows.
global.TabContext = function TabContext(getDefaults, extension) {
  this.extension = extension;
  this.getDefaults = getDefaults;

  this.tabData = new WeakMap();
  this.lastLocation = new WeakMap();

  windowTracker.addListener("progress", this);
  windowTracker.addListener("TabSelect", this);

  EventEmitter.decorate(this);
};

TabContext.prototype = {
  get(tab) {
    if (!this.tabData.has(tab)) {
      this.tabData.set(tab, this.getDefaults(tab));
    }

    return this.tabData.get(tab);
  },

  clear(tab) {
    this.tabData.delete(tab);
  },

  handleEvent(event) {
    if (event.type == "TabSelect") {
      let tab = event.target;
      this.emit("tab-select", tab);
      this.emit("location-change", tab);
    }
  },

  onStateChange(browser, webProgress, request, stateFlags, statusCode) {
    let flags = Ci.nsIWebProgressListener;

    if (!(~stateFlags & (flags.STATE_IS_WINDOW | flags.STATE_START) ||
          this.lastLocation.has(browser))) {
      this.lastLocation.set(browser, request.URI);
    }
  },

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    let gBrowser = browser.ownerGlobal.gBrowser;
    let lastLocation = this.lastLocation.get(browser);
    if (browser === gBrowser.selectedBrowser &&
        !(lastLocation && lastLocation.equalsExceptRef(browser.currentURI))) {
      let tab = gBrowser.getTabForBrowser(browser);
      this.emit("location-change", tab, true);
    }
    this.lastLocation.set(browser, browser.currentURI);
  },

  shutdown() {
    windowTracker.removeListener("progress", this);
    windowTracker.removeListener("TabSelect", this);
  },
};

function getBrowserInfo(browser) {
  if (!browser.ownerGlobal.gBrowser) {
    // When we're loaded into a <browser> inside about:addons, we need to go up
    // one more level.
    browser = browser.ownerGlobal.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDocShell)
                     .chromeEventHandler;

    if (!browser) {
      return {};
    }
  }

  let result = {};

  let window = browser.ownerGlobal;
  if (window.gBrowser) {
    let tab = window.gBrowser.getTabForBrowser(browser);
    if (tab) {
      result.tabId = tabTracker.getId(tab);
    }

    result.windowId = windowTracker.getId(window);
  }

  return result;
}
global.getBrowserInfo = getBrowserInfo;

// Sends the tab and windowId upon request. This is primarily used to support
// the synchronous `browser.extension.getViews` API.
let onGetTabAndWindowId = {
  receiveMessage({name, target, sync}) {
    let result = getBrowserInfo(target);

    if (result.tabId) {
      if (sync) {
        return result;
      }
      target.messageManager.sendAsyncMessage("Extension:SetTabAndWindowId", result);
    }
  },
};
/* eslint-disable mozilla/balanced-listeners */
Services.mm.addMessageListener("Extension:GetTabAndWindowId", onGetTabAndWindowId);
/* eslint-enable mozilla/balanced-listeners */


class WindowTracker extends WindowTrackerBase {
  addProgressListener(window, listener) {
    window.gBrowser.addTabsProgressListener(listener);
  }

  removeProgressListener(window, listener) {
    window.gBrowser.removeTabsProgressListener(listener);
  }
}

global.WindowEventManager = class extends SingletonEventManager {
  constructor(context, name, event, listener) {
    super(context, name, fire => {
      let listener2 = listener.bind(null, fire);

      windowTracker.addListener(event, listener2);
      return () => {
        windowTracker.removeListener(event, listener2);
      };
    });
  }
};

class TabTracker extends TabTrackerBase {
  constructor() {
    super();

    this._tabs = new WeakMap();
    this._tabIds = new Map();
    this._nextId = 1;

    this._handleTabDestroyed = this._handleTabDestroyed.bind(this);
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    this.adoptedTabs = new WeakMap();

    this._handleWindowOpen = this._handleWindowOpen.bind(this);
    this._handleWindowClose = this._handleWindowClose.bind(this);

    windowTracker.addListener("TabClose", this);
    windowTracker.addListener("TabOpen", this);
    windowTracker.addOpenListener(this._handleWindowOpen);
    windowTracker.addCloseListener(this._handleWindowClose);

    /* eslint-disable mozilla/balanced-listeners */
    this.on("tab-detached", this._handleTabDestroyed);
    this.on("tab-removed", this._handleTabDestroyed);
    /* eslint-enable mozilla/balanced-listeners */
  }

  getId(tab) {
    if (this._tabs.has(tab)) {
      return this._tabs.get(tab);
    }

    this.init();

    let id = this._nextId++;
    this.setId(tab, id);
    return id;
  }

  setId(tab, id) {
    this._tabs.set(tab, id);
    this._tabIds.set(id, tab);
  }

  _handleTabDestroyed(event, {tab}) {
    let id = this._tabs.get(tab);
    if (id) {
      this._tabs.delete(tab);
      if (this._tabIds.get(id) === tab) {
        this._tabIds.delete(id);
      }
    }
  }

  /**
   * Returns the XUL <tab> element associated with the given tab ID. If no tab
   * with the given ID exists, and no default value is provided, an error is
   * raised, belonging to the scope of the given context.
   *
   * @param {integer} tabId
   *        The ID of the tab to retrieve.
   * @param {*} default_
   *        The value to return if no tab exists with the given ID.
   * @returns {Element<tab>}
   *        A XUL <tab> element.
   */
  getTab(tabId, default_ = undefined) {
    let tab = this._tabIds.get(tabId);
    if (tab) {
      return tab;
    }
    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${tabId}`);
  }

  handleEvent(event) {
    let tab = event.target;

    switch (event.type) {
      case "TabOpen":
        let {adoptedTab} = event.detail;
        if (adoptedTab) {
          this.adoptedTabs.set(adoptedTab, event.target);

          // This tab is being created to adopt a tab from a different window.
          // Copy the ID from the old tab to the new.
          this.setId(tab, this.getId(adoptedTab));

          adoptedTab.linkedBrowser.messageManager.sendAsyncMessage("Extension:SetTabAndWindowId", {
            windowId: windowTracker.getId(tab.ownerGlobal),
          });
        }

        // We need to delay sending this event until the next tick, since the
        // tab does not have its final index when the TabOpen event is dispatched.
        Promise.resolve().then(() => {
          if (event.detail.adoptedTab) {
            this.emitAttached(event.originalTarget);
          } else {
            this.emitCreated(event.originalTarget);
          }
        });
        break;

      case "TabClose":
        let {adoptedBy} = event.detail;
        if (adoptedBy) {
          // This tab is being closed because it was adopted by a new window.
          // Copy its ID to the new tab, in case it was created as the first tab
          // of a new window, and did not have an `adoptedTab` detail when it was
          // opened.
          this.setId(adoptedBy, this.getId(tab));

          this.emitDetached(tab, adoptedBy);
        } else {
          this.emitRemoved(tab, false);
        }
        break;
    }
  }

  _handleWindowOpen(window) {
    if (window.arguments && window.arguments[0] instanceof window.XULElement) {
      // If the first window argument is a XUL element, it means the
      // window is about to adopt a tab from another window to replace its
      // initial tab.
      //
      // Note that this event handler depends on running before the
      // delayed startup code in browser.js, which is currently triggered
      // by the first MozAfterPaint event. That code handles finally
      // adopting the tab, and clears it from the arguments list in the
      // process, so if we run later than it, we're too late.
      let tab = window.arguments[0];
      let adoptedBy = window.gBrowser.tabs[0];

      this.adoptedTabs.set(tab, adoptedBy);
      this.setId(adoptedBy, this.getId(tab));

      // We need to be sure to fire this event after the onDetached event
      // for the original tab.
      let listener = (event, details) => {
        if (details.tab === tab) {
          this.off("tab-detached", listener);

          Promise.resolve().then(() => {
            this.emitAttached(details.adoptedBy);
          });
        }
      };

      this.on("tab-detached", listener);
    } else {
      for (let tab of window.gBrowser.tabs) {
        this.emitCreated(tab);
      }
    }
  }

  _handleWindowClose(window) {
    for (let tab of window.gBrowser.tabs) {
      if (this.adoptedTabs.has(tab)) {
        this.emitDetached(tab, this.adoptedTabs.get(tab));
      } else {
        this.emitRemoved(tab, true);
      }
    }
  }

  emitAttached(tab) {
    let newWindowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    this.emit("tab-attached", {tab, tabId, newWindowId, newPosition: tab._tPos});
  }

  emitDetached(tab, adoptedBy) {
    let oldWindowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    this.emit("tab-detached", {tab, adoptedBy, tabId, oldWindowId, oldPosition: tab._tPos});
  }

  emitCreated(tab) {
    this.emit("tab-created", {tab});
  }

  emitRemoved(tab, isWindowClosing) {
    let windowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    // When addons run in-process, `window.close()` is synchronous. Most other
    // addon-invoked calls are asynchronous since they go through a proxy
    // context via the message manager. This includes event registrations such
    // as `tabs.onRemoved.addListener`.
    //
    // So, even if `window.close()` were to be called (in-process) after calling
    // `tabs.onRemoved.addListener`, then the tab would be closed before the
    // event listener is registered. To make sure that the event listener is
    // notified, we dispatch `tabs.onRemoved` asynchronously.
    Services.tm.mainThread.dispatch(() => {
      this.emit("tab-removed", {tab, tabId, windowId, isWindowClosing});
    }, Ci.nsIThread.DISPATCH_NORMAL);
  }

  getBrowserId(browser) {
    let {gBrowser} = browser.ownerGlobal;
    // Some non-browser windows have gBrowser but not
    // getTabForBrowser!
    if (gBrowser && gBrowser.getTabForBrowser) {
      let tab = gBrowser.getTabForBrowser(browser);
      if (tab) {
        return this.getId(tab);
      }
    }
    return -1;
  }

  get activeTab() {
    let window = windowTracker.topWindow;
    if (window && window.gBrowser) {
      return window.gBrowser.selectedTab;
    }
    return null;
  }
}

windowTracker = new WindowTracker();
tabTracker = new TabTracker();

Object.assign(global, {tabTracker, windowTracker});

class Tab extends TabBase {
  get _favIconUrl() {
    return this.window.gBrowser.getIcon(this.tab);
  }

  get audible() {
    return this.tab.soundPlaying;
  }

  get browser() {
    return this.tab.linkedBrowser;
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.tab);
  }

  get height() {
    return this.browser.clientHeight;
  }

  get index() {
    return this.tab._tPos;
  }

  get innerWindowID() {
    return this.browser.innerWindowID;
  }

  get mutedInfo() {
    let tab = this.tab;

    let mutedInfo = {muted: tab.muted};
    if (tab.muteReason === null) {
      mutedInfo.reason = "user";
    } else if (tab.muteReason) {
      mutedInfo.reason = "extension";
      mutedInfo.extensionId = tab.muteReason;
    }

    return mutedInfo;
  }

  get pinned() {
    return this.tab.pinned;
  }

  get active() {
    return this.tab.selected;
  }

  get selected() {
    return this.tab.selected;
  }

  get status() {
    if (this.tab.getAttribute("busy") === "true") {
      return "loading";
    }
    return "complete";
  }

  get width() {
    return this.browser.clientWidth;
  }

  get window() {
    return this.tab.ownerGlobal;
  }

  get windowId() {
    return windowTracker.getId(this.window);
  }

  static convertFromSessionStoreClosedData(extension, tab, window = null) {
    let result = {
      sessionId: String(tab.closedId),
      index: tab.pos ? tab.pos : 0,
      windowId: window && windowTracker.getId(window),
      selected: false,
      highlighted: false,
      active: false,
      pinned: false,
      incognito: Boolean(tab.state && tab.state.isPrivate),
    };

    if (extension.tabManager.hasTabPermission(tab)) {
      let entries = tab.state ? tab.state.entries : tab.entries;
      result.url = entries[0].url;
      result.title = entries[0].title;
      if (tab.image) {
        result.favIconUrl = tab.image;
      }
    }

    return result;
  }
}

class Window extends WindowBase {
  updateGeometry(options) {
    let {window} = this;

    if (options.left !== null || options.top !== null) {
      let left = options.left !== null ? options.left : window.screenX;
      let top = options.top !== null ? options.top : window.screenY;
      window.moveTo(left, top);
    }

    if (options.width !== null || options.height !== null) {
      let width = options.width !== null ? options.width : window.outerWidth;
      let height = options.height !== null ? options.height : window.outerHeight;
      window.resizeTo(width, height);
    }
  }

  get focused() {
    return this.window.document.hasFocus();
  }

  get top() {
    return this.window.screenY;
  }

  get left() {
    return this.window.screenX;
  }

  get width() {
    return this.window.outerWidth;
  }

  get height() {
    return this.window.outerHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isWindowPrivate(this.window);
  }

  get alwaysOnTop() {
    return this.xulWindow.zLevel >= Ci.nsIXULWindow.raisedZ;
  }

  get isLastFocused() {
    return this.window === windowTracker.topWindow;
  }

  static getState(window) {
    const STATES = {
      [window.STATE_MAXIMIZED]: "maximized",
      [window.STATE_MINIMIZED]: "minimized",
      [window.STATE_NORMAL]: "normal",
    };
    let state = STATES[window.windowState];
    if (window.fullScreen) {
      state = "fullscreen";
    }
    return state;
  }

  get state() {
    return Window.getState(this.window);
  }

  set state(state) {
    let {window} = this;
    if (state !== "fullscreen" && window.fullScreen) {
      window.fullScreen = false;
    }

    switch (state) {
      case "maximized":
        window.maximize();
        break;

      case "minimized":
      case "docked":
        window.minimize();
        break;

      case "normal":
        // Restore sometimes returns the window to its previous state, rather
        // than to the "normal" state, so it may need to be called anywhere from
        // zero to two times.
        window.restore();
        if (window.windowState !== window.STATE_NORMAL) {
          window.restore();
        }
        if (window.windowState !== window.STATE_NORMAL) {
          // And on OS-X, where normal vs. maximized is basically a heuristic,
          // we need to cheat.
          window.sizeToContent();
        }
        break;

      case "fullscreen":
        window.fullScreen = true;
        break;

      default:
        throw new Error(`Unexpected window state: ${state}`);
    }
  }

  * getTabs() {
    let {tabManager} = this.extension;

    for (let tab of this.window.gBrowser.tabs) {
      yield tabManager.getWrapper(tab);
    }
  }

  static convertFromSessionStoreClosedData(extension, window) {
    let result = {
      sessionId: String(window.closedId),
      focused: false,
      incognito: false,
      type: "normal", // this is always "normal" for a closed window
      // Surely this does not actually work?
      state: this.getState(window),
      alwaysOnTop: false,
    };

    if (window.tabs.length) {
      result.tabs = window.tabs.map(tab => {
        return Tab.convertFromSessionStoreClosedData(extension, tab);
      });
    }

    return result;
  }
}

Object.assign(global, {Tab, Window});

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    let tab = tabTracker.getTab(tabId, default_);

    if (tab) {
      return this.getWrapper(tab);
    }
    return default_;
  }

  addActiveTabPermission(tab = tabTracker.activeTab) {
    return super.addActiveTabPermission(tab);
  }

  revokeActiveTabPermission(tab = tabTracker.activeTab) {
    return super.revokeActiveTabPermission(tab);
  }

  wrapTab(tab) {
    return new Tab(this.extension, tab, tabTracker.getId(tab));
  }
}

class WindowManager extends WindowManagerBase {
  get(windowId, context) {
    let window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  * getAll() {
    for (let window of windowTracker.browserWindows()) {
      yield this.getWrapper(window);
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}


extensions.on("startup", (type, extension) => { // eslint-disable-line mozilla/balanced-listeners
  defineLazyGetter(extension, "tabManager",
                   () => new TabManager(extension));
  defineLazyGetter(extension, "windowManager",
                   () => new WindowManager(extension));
});
