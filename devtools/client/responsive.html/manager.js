/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const { Task } = require("resource://gre/modules/Task.jsm");
const EventEmitter = require("devtools/shared/event-emitter");

const TOOL_URL = "chrome://devtools/content/responsive.html/index.xhtml";

/**
 * ResponsiveUIManager is the external API for the browser UI, etc. to use when
 * opening and closing the responsive UI.
 *
 * While the HTML UI is in an experimental stage, the older ResponsiveUIManager
 * from devtools/client/responsivedesign/responsivedesign.jsm delegates to this
 * object when the pref "devtools.responsive.html.enabled" is true.
 */
exports.ResponsiveUIManager = {
  activeTabs: new Map(),

  /**
   * Toggle the responsive UI for a tab.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   * @return Promise
   *         Resolved when the toggling has completed.
   */
  toggle(window, tab) {
    if (this.isActiveForTab(tab)) {
      this.activeTabs.get(tab).destroy();
      this.activeTabs.delete(tab);
    } else {
      this.runIfNeeded(window, tab);
    }
    // TODO: Becomes a more interesting value in a later patch
    return promise.resolve();
  },

  /**
   * Launches the responsive UI.
   *
   * @param window
   *        The main browser chrome window.
   * @param tab
   *        The browser tab.
   */
  runIfNeeded(window, tab) {
    if (!this.isActiveForTab(tab)) {
      this.activeTabs.set(tab, new ResponsiveUI(window, tab));
    }
  },

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
  handleGcliCommand: function(window, tab, command, args) {
    switch (command) {
      case "resize to":
        this.runIfNeeded(window, tab);
        // TODO: Probably the wrong API
        this.activeTabs.get(tab).setSize(args.width, args.height);
        break;
      case "resize on":
        this.runIfNeeded(window, tab);
        break;
      case "resize off":
        if (this.isActiveForTab(tab)) {
          this.activeTabs.get(tab).destroy();
          this.activeTabs.delete(tab);
        }
        break;
      case "resize toggle":
        this.toggle(window, tab);
        break;
      default:
    }
  }
};

// GCLI commands in ../responsivedesign/resize-commands.js listen for events
// from this object to know when the UI for a tab has opened or closed.
EventEmitter.decorate(exports.ResponsiveUIManager);

/**
 * ResponsiveUI manages the responsive design tool for a specific tab.  The
 * actual tool itself lives in a separate chrome:// document that is loaded into
 * the tab upon opening responsive design.  This object acts a helper to
 * integrate the tool into the surrounding browser UI as needed.
 */
function ResponsiveUI(window, tab) {
  this.window = window;
  this.tab = tab;
  this.init();
}

ResponsiveUI.prototype = {

  /**
   * The main browser chrome window (that holds many tabs).
   */
  window: null,

  /**
   * The specific browser tab this responsive instance is for.
   */
  tab: null,

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
  init: Task.async(function*() {
    let tabBrowser = this.tab.linkedBrowser;
    let contentURI = tabBrowser.documentURI.spec;
    tabBrowser.loadURI(TOOL_URL);
    yield tabLoaded(this.tab);
    let toolWindow = tabBrowser.contentWindow;
    toolWindow.addViewport(contentURI);
  }),

  destroy() {
    let tabBrowser = this.tab.linkedBrowser;
    tabBrowser.goBack();
    this.window = null;
    this.tab = null;
  },

};

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
