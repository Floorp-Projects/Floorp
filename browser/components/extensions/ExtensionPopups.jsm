/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported PanelPopup, ViewPopup */

var EXPORTED_SYMBOLS = ["BasePopup", "PanelPopup", "ViewPopup"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "CustomizableUI",
                               "resource:///modules/CustomizableUI.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionParent",
                               "resource://gre/modules/ExtensionParent.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
                               "resource://gre/modules/Timer.jsm");

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  DefaultWeakMap,
  promiseEvent,
} = ExtensionUtils;

const {
  makeWidgetId,
} = ExtensionCommon;


const POPUP_LOAD_TIMEOUT_MS = 200;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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

const REMOTE_PANEL_ID = "webextension-remote-preload-panel";

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
    this.panel.addEventListener("popuppositioned", this, {once: true, capture: true});

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
        this.browser.parentNode.remove();
      }
      if (this.stack) {
        this.stack.remove();
      }

      if (this.viewNode) {
        this.viewNode.removeEventListener(this.DESTROY_EVENT, this);
        delete this.viewNode.customRectGetter;
      }

      let {panel} = this;
      if (panel) {
        panel.removeEventListener("popuppositioned", this, {capture: true});
      }
      if (panel && panel.id !== REMOTE_PANEL_ID) {
        panel.style.removeProperty("--arrowpanel-background");
        panel.style.removeProperty("--arrowpanel-border-color");
        panel.removeAttribute("remote");
      }

      this.browser = null;
      this.stack = null;
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
      case "popuppositioned":
        if (!this.destroyed) {
          this.browserLoaded.then(() => {
            if (this.destroyed) {
              return;
            }
            this.browser.messageManager.sendAsyncMessage("Extension:GrabFocus", {});
          }).catch(() => {
            // If the panel closes too fast an exception is raised here and tests will fail.
          });
        }
        break;
    }
  }

  createBrowser(viewNode, popupURL = null) {
    let document = viewNode.ownerDocument;

    let stack = document.createElementNS(XUL_NS, "stack");
    stack.setAttribute("class", "webextension-popup-stack");

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
    browser.setAttribute("selectmenuconstrained", "false");
    browser.sameProcessAsFrameLoader = this.extension.groupFrameLoader;

    if (this.extension.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
    }

    // We only need flex sizing for the sake of the slide-in sub-views of the
    // main menu panel, so that the browser occupies the full width of the view,
    // and also takes up any extra height that's available to it.
    browser.setAttribute("flex", "1");
    stack.setAttribute("flex", "1");

    // Note: When using noautohide panels, the popup manager will add width and
    // height attributes to the panel, breaking our resize code, if the browser
    // starts out smaller than 30px by 10px. This isn't an issue now, but it
    // will be if and when we popup debugging.

    this.browser = browser;
    this.stack = stack;

    let readyPromise;
    if (this.extension.remote) {
      readyPromise = promiseEvent(browser, "XULFrameLoaderCreated");
    } else {
      readyPromise = promiseEvent(browser, "load");
    }

    stack.appendChild(browser);
    viewNode.appendChild(stack);

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

      browser.loadURI(popupURL, {triggeringPrincipal: this.extension.principal});
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

      // Used by the panelmultiview code to figure out sizing without reparenting
      // (which would destroy the browser and break us).
      this.lastCalculatedInViewHeight = Math.max(height, this.viewHeight);
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
    // Panels inherit the applied theme (light, dark, etc) and there is a high
    // likelihood that most extension authors will not have tested with a dark theme.
    // If they have not set a background-color, we force it to white to ensure visibility
    // of the extension content. Passing `null` should be treated the same as no argument,
    // which is why we can't use default parameters here.
    if (!background) {
      background = "#fff";
    }
    if (this.panel.id != "widget-overflow") {
      this.panel.style.setProperty("--arrowpanel-background", background);
    }
    if (background == "#fff") {
      // Set a usable default color that work with the default background-color.
      this.panel.style.setProperty("--arrowpanel-border-color", "hsla(210,4%,10%,.15)");
    }
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
  constructor(extension, document, popupURL, browserStyle) {
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

    panel.addEventListener("popupshowing", () => {
      let event = new this.window.CustomEvent("WebExtPopupLoaded", {
        bubbles: true,
        detail: {extension},
      });
      this.browser.dispatchEvent(event);
    }, {once: true});

    super(extension, panel, popupURL, browserStyle);
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

    let createPanel = remote => {
      let panel = document.createElement("panel");
      panel.setAttribute("type", "arrow");
      if (remote) {
        panel.setAttribute("remote", "true");
      }

      document.getElementById("mainPopupSet").appendChild(panel);
      return panel;
    };

    // Create a temporary panel to hold the browser while it pre-loads its
    // content. This panel will never be shown, but the browser's docShell will
    // be swapped with the browser in the real panel when it's ready. For remote
    // extensions, this popup is shared between all extensions.
    let panel;
    if (extension.remote) {
      panel = document.getElementById(REMOTE_PANEL_ID);
      if (!panel) {
        panel = createPanel(true);
        panel.id = REMOTE_PANEL_ID;
      }
    } else {
      panel = createPanel();
    }

    super(extension, panel, popupURL, browserStyle, fixedWidth, blockParser);

    this.ignoreResizes = true;

    this.attached = false;
    this.shown = false;
    this.tempPanel = panel;
    this.tempBrowser = this.browser;

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
    this.viewNode.setAttribute("closemenu", "none");

    this.panel.addEventListener("popuppositioned", this, {once: true, capture: true});
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

    if (this.dimensions) {
      if (this.fixedWidth) {
        delete this.dimensions.width;
      }
      this.resizeBrowser(this.dimensions);
    }

    this.viewNode.customRectGetter = () => {
      return {height: this.lastCalculatedInViewHeight || this.viewHeight};
    };

    this.removeTempPanel();

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

  removeTempPanel() {
    if (this.tempPanel) {
      if (this.tempPanel.id !== REMOTE_PANEL_ID) {
        this.tempPanel.remove();
      }
      this.tempPanel = null;
    }
    if (this.tempBrowser) {
      this.tempBrowser.parentNode.remove();
      this.tempBrowser = null;
    }
  }

  destroy() {
    return super.destroy().then(() => {
      this.removeTempPanel();
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
