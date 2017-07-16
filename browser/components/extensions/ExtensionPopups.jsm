/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported PanelPopup, ViewPopup */

var EXPORTED_SYMBOLS = ["BasePopup", "PanelPopup", "ViewPopup"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionParent",
                                  "resource://gre/modules/ExtensionParent.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  DefaultWeakMap,
  promiseEvent,
} = ExtensionUtils;


const POPUP_LOAD_TIMEOUT_MS = 200;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function makeWidgetId(id) {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
}

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
  constructor(extension, viewNode, popupURL, browserStyle, fixedWidth = false, blockParser = false) {
    this.extension = extension;
    this.popupURL = popupURL;
    this.viewNode = viewNode;
    this.browserStyle = browserStyle;
    this.window = viewNode.ownerGlobal;
    this.destroyed = false;
    this.fixedWidth = fixedWidth;
    this.blockParser = blockParser;

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
    // Ignore unhandled rejections if the "attach" method is not called.
    this.browserLoaded.catch(() => {});

    BasePopup.instances.get(this.window).delete(this.extension);

    return this.browserReady.then(() => {
      if (this.browser) {
        this.destroyBrowser(this.browser, true);
        this.browser.remove();
      }

      if (this.viewNode) {
        this.viewNode.removeEventListener(this.DESTROY_EVENT, this);
        this.viewNode.style.maxHeight = "";
      }

      let {panel} = this;
      if (panel) {
        panel.style.removeProperty("--arrowpanel-background");
        panel.style.removeProperty("--panel-arrow-image-vertical");
        panel.removeAttribute("remote");
      }

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
      sheets.push(...ExtensionParent.extensionStylesheets);
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
    browser.setAttribute("contextmenu", "contentAreaContextMenu");
    browser.setAttribute("autocompletepopup", "PopupAutoComplete");
    browser.setAttribute("selectmenulist", "ContentSelectDropdown");
    browser.sameProcessAsFrameLoader = this.extension.groupFrameLoader;

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

    ExtensionParent.apiManager.emit("extension-browser-inserted", browser);

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

      // Sets the context information for context menus.
      mm.loadFrameScript("chrome://browser/content/content.js", true);

      mm.loadFrameScript(
        "chrome://extensions/content/ext-browser-content.js", false);

      mm.sendAsyncMessage("Extension:InitBrowser", {
        allowScriptsToClose: true,
        blockParser: this.blockParser,
        fixedWidth: this.fixedWidth,
        maxWidth: 800,
        maxHeight: 600,
        stylesheets: this.STYLESHEETS,
      });

      browser.loadURI(popupURL);
    });
  }

  unblockParser() {
    this.browserReady.then(browser => {
      this.browser.messageManager.sendAsyncMessage("Extension:UnblockParser");
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
    if (extension.remote) {
      panel.setAttribute("remote", "true");
    }

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
  constructor(extension, window, popupURL, browserStyle, fixedWidth, blockParser) {
    let document = window.document;

    // Create a temporary panel to hold the browser while it pre-loads its
    // content. This panel will never be shown, but the browser's docShell will
    // be swapped with the browser in the real panel when it's ready.
    let panel = document.createElement("panel");
    panel.setAttribute("type", "arrow");
    if (extension.remote) {
      panel.setAttribute("remote", "true");
    }
    document.getElementById("mainPopupSet").appendChild(panel);

    super(extension, panel, popupURL, browserStyle, fixedWidth, blockParser);

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
  async attach(viewNode) {
    this.viewNode = viewNode;
    this.viewNode.addEventListener(this.DESTROY_EVENT, this);

    if (this.extension.remote) {
      this.panel.setAttribute("remote", "true");
    }

    // Wait until the browser element is fully initialized, and give it at least
    // a short grace period to finish loading its initial content, if necessary.
    //
    // In practice, the browser that was created by the mousdown handler should
    // nearly always be ready by this point.
    await Promise.all([
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
    await this.createBrowser(this.viewNode);

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
