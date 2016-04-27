/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cr } = require("chrome");
const promise = require("promise");
const { Task } = require("resource://gre/modules/Task.jsm");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const EventEmitter = require("devtools/shared/event-emitter");
const { getOwnerWindow } = require("sdk/tabs/utils");
const { on, off } = require("sdk/event/core");
const { startup } = require("sdk/window/helpers");
const events = require("./events");

const TOOL_URL = "chrome://devtools/content/responsive.html/index.xhtml";

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 *
 * While the HTML UI is in an experimental stage, the older ResponsiveUIManager
 * from devtools/client/responsivedesign/responsivedesign.jsm delegates to this
 * object when the pref "devtools.responsive.html.enabled" is true.
 */
const ResponsiveUIManager = exports.ResponsiveUIManager = {
  activeTabs: new Map(),

  /**
   * Toggle the responsive UI for a tab.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @return Promise
   *         Resolved when the toggling has completed.  If the UI has opened,
   *         it is resolved to the ResponsiveUI instance for this tab.  If the
   *         the UI has closed, there is no resolution value.
   */
  toggle(window, tab) {
    let action = this.isActiveForTab(tab) ? "close" : "open";
    return this[action + "IfNeeded"](window, tab);
  },

  /**
   * Opens the responsive UI, if not already open.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @return Promise
   *         Resolved to the ResponsiveUI instance for this tab when opening is
   *         complete.
   */
  openIfNeeded: Task.async(function* (window, tab) {
    if (!this.isActiveForTab(tab)) {
      if (!this.activeTabs.size) {
        on(events.activate, "data", onActivate);
        on(events.close, "data", onClose);
      }
      let ui = new ResponsiveUI(window, tab);
      this.activeTabs.set(tab, ui);
      yield setMenuCheckFor(tab, window);
      yield ui.inited;
      this.emit("on", { tab });
    }
    return this.getResponsiveUIForTab(tab);
  }),

  /**
   * Closes the responsive UI, if not already closed.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @return Promise
   *         Resolved (with no value) when closing is complete.
   */
  closeIfNeeded: Task.async(function* (window, tab) {
    if (this.isActiveForTab(tab)) {
      let ui = this.activeTabs.get(tab);
      this.activeTabs.delete(tab);

      if (!this.activeTabs.size) {
        off(events.activate, "data", onActivate);
        off(events.close, "data", onClose);
      }

      yield ui.destroy();
      this.emit("off", { tab });

      yield setMenuCheckFor(tab, window);
    }
    return promise.resolve();
  }),

  /**
   * Returns true if responsive UI is active for a given tab.
   *
   * @param tab
   *        The browser tab.
   * @return boolean
   */
  isActiveForTab(tab) {
    return this.activeTabs.has(tab);
  },

  /**
   * Return the responsive UI controller for a tab.
   *
   * @param tab
   *        The browser tab.
   * @return ResponsiveUI
   *         The UI instance for this tab.
   */
  getResponsiveUIForTab(tab) {
    return this.activeTabs.get(tab);
  },

  /**
   * Handle GCLI commands.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @param command
   *        The GCLI command name.
   * @param args
   *        The GCLI command arguments.
   */
  handleGcliCommand: function (window, tab, command, args) {
    let completed;
    switch (command) {
      case "resize to":
        completed = this.openIfNeeded(window, tab);
        this.activeTabs.get(tab).setViewportSize(args.width, args.height);
        break;
      case "resize on":
        completed = this.openIfNeeded(window, tab);
        break;
      case "resize off":
        completed = this.closeIfNeeded(window, tab);
        break;
      case "resize toggle":
        completed = this.toggle(window, tab);
        break;
      default:
    }
    completed.catch(e => console.error(e));
  }
};

// GCLI commands in ../responsivedesign/resize-commands.js listen for events
// from this object to know when the UI for a tab has opened or closed.
EventEmitter.decorate(ResponsiveUIManager);

/**
 * ResponsiveUI manages the responsive design tool for a specific tab.  The
 * actual tool itself lives in a separate chrome:// document that is loaded into
 * the tab upon opening responsive design.  This object acts a helper to
 * integrate the tool into the surrounding browser UI as needed.
 */
function ResponsiveUI(window, tab) {
  this.browserWindow = window;
  this.tab = tab;
  this.inited = this.init();
}

ResponsiveUI.prototype = {

  /**
   * The main browser chrome window (that holds many tabs).
   */
  browserWindow: null,

  /**
   * The specific browser tab this responsive instance is for.
   */
  tab: null,

  /**
   * Promise resovled when the UI init has completed.
   */
  inited: null,

  /**
   * A window reference for the chrome:// document that displays the responsive
   * design tool.  It is safe to reference this window directly even with e10s,
   * as the tool UI is always loaded in the parent process.  The web content
   * contained *within* the tool UI on the other hand is loaded in the child
   * process.
   */
  toolWindow: null,

  /**
   * For the moment, we open the tool by:
   * 1. Recording the tab's URL
   * 2. Navigating the tab to the tool
   * 3. Passing along the URL to the tool to open in the viewport
   *
   * This approach is simple, but it also discards the user's state on the page.
   * It's just like opening a fresh tab and pasting the URL.
   *
   * In the future, we can do better by using swapFrameLoaders to preserve the
   * state.  Platform discussions are in progress to make this happen.  See
   * bug 1238160 about <iframe mozbrowser> for more details.
   */
  init: Task.async(function* () {
    let tabBrowser = this.tab.linkedBrowser;
    let contentURI = tabBrowser.documentURI.spec;
    tabBrowser.loadURI(TOOL_URL);
    yield tabLoaded(this.tab);
    let toolWindow = this.toolWindow = tabBrowser.contentWindow;
    toolWindow.addEventListener("message", this);
    yield waitForMessage(toolWindow, "init");
    toolWindow.addInitialViewport(contentURI);
    yield waitForMessage(toolWindow, "browser-mounted");
  }),

  destroy: Task.async(function* () {
    let tabBrowser = this.tab.linkedBrowser;
    let browserWindow = this.browserWindow;
    this.browserWindow = null;
    this.tab = null;
    this.inited = null;
    this.toolWindow = null;
    let loaded = waitForDocLoadComplete(browserWindow.gBrowser);
    tabBrowser.goBack();
    yield loaded;
  }),

  handleEvent(event) {
    let { tab, window } = this;
    let toolWindow = tab.linkedBrowser.contentWindow;

    if (event.origin !== "chrome://devtools") {
      return;
    }

    switch (event.data.type) {
      case "content-resize":
        let { width, height } = event.data;
        this.emit("content-resize", {
          width,
          height,
        });
        break;
      case "exit":
        toolWindow.removeEventListener(event.type, this);
        ResponsiveUIManager.closeIfNeeded(window, tab);
        break;
    }
  },

  getViewportSize() {
    return this.toolWindow.getViewportSize();
  },

  setViewportSize: Task.async(function* (width, height) {
    yield this.inited;
    this.toolWindow.setViewportSize(width, height);
  }),

  getViewportMessageManager() {
    return this.toolWindow.getViewportMessageManager();
  },

};

EventEmitter.decorate(ResponsiveUI.prototype);

function waitForMessage(win, type) {
  let deferred = promise.defer();

  let onMessage = event => {
    if (event.data.type !== type) {
      return;
    }
    win.removeEventListener("message", onMessage);
    deferred.resolve();
  };
  win.addEventListener("message", onMessage);

  return deferred.promise;
}

function tabLoaded(tab) {
  let deferred = promise.defer();

  function handle(event) {
    if (event.originalTarget != tab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      return;
    }
    tab.linkedBrowser.removeEventListener("load", handle, true);
    deferred.resolve(event);
  }

  tab.linkedBrowser.addEventListener("load", handle, true);
  return deferred.promise;
}

/**
 * Waits for the next load to complete in the current browser.
 */
function waitForDocLoadComplete(gBrowser) {
  let deferred = promise.defer();
  let progressListener = {
    onStateChange: function (webProgress, req, flags, status) {
      let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                    Ci.nsIWebProgressListener.STATE_STOP;

      // When a load needs to be retargetted to a new process it is cancelled
      // with NS_BINDING_ABORTED so ignore that case
      if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
        gBrowser.removeProgressListener(progressListener);
        deferred.resolve();
      }
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference])
  };
  gBrowser.addProgressListener(progressListener);
  return deferred.promise;
}

const onActivate = (tab) => setMenuCheckFor(tab);

const onClose = ({ window, tabs }) => {
  for (let tab of tabs) {
    ResponsiveUIManager.closeIfNeeded(window, tab);
  }
};

const setMenuCheckFor = Task.async(
  function* (tab, window = getOwnerWindow(tab)) {
    yield startup(window);

    let menu = window.document.getElementById("menu_responsiveUI");
    menu.setAttribute("checked", ResponsiveUIManager.isActiveForTab(tab));
  }
);
