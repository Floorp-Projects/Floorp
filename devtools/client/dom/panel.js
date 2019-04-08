/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const ObjectClient = require("devtools/shared/client/object-client");

const EventEmitter = require("devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "openContentLink", "devtools/client/shared/link", true);

/**
 * This object represents DOM panel. It's responsibility is to
 * render Document Object Model of the current debugger target.
 */
function DomPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this.onTabNavigated = this.onTabNavigated.bind(this);
  this.onContentMessage = this.onContentMessage.bind(this);
  this.onPanelVisibilityChange = this.onPanelVisibilityChange.bind(this);

  this.pendingRequests = new Map();

  EventEmitter.decorate(this);
}

DomPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the DOM panel completes opening.
   */
  open() {
    this.initialize();

    this.isReady = true;
    this.emit("ready");

    return this;
  },

  // Initialization

  initialize: function() {
    this.panelWin.addEventListener("devtools/content/message",
      this.onContentMessage, true);

    this.target.on("navigate", this.onTabNavigated);
    this._toolbox.on("select", this.onPanelVisibilityChange);

    // Export provider object with useful API for DOM panel.
    const provider = {
      getToolbox: this.getToolbox.bind(this),
      getPrototypeAndProperties: this.getPrototypeAndProperties.bind(this),
      openLink: this.openLink.bind(this),
    };

    exportIntoContentScope(this.panelWin, provider, "DomProvider");

    this.shouldRefresh = true;
  },

  destroy() {
    if (this._destroying) {
      return this._destroying;
    }

    this._destroying = new Promise(resolve => {
      this.target.off("navigate", this.onTabNavigated);
      this._toolbox.off("select", this.onPanelVisibilityChange);

      this.emit("destroyed");
      resolve();
    });

    return this._destroying;
  },

  // Events

  refresh: function() {
    // Do not refresh if the panel isn't visible.
    if (!this.isPanelVisible()) {
      return;
    }

    // Do not refresh if it isn't necessary.
    if (!this.shouldRefresh) {
      return;
    }

    // Alright reset the flag we are about to refresh the panel.
    this.shouldRefresh = false;

    this.getRootGrip().then(rootGrip => {
      this.postContentMessage("initialize", rootGrip);
    });
  },

  /**
   * Make sure the panel is refreshed when the page is reloaded.
   * The panel is refreshed immediately if it's currently selected
   * or lazily  when the user actually selects it.
   */
  onTabNavigated: function() {
    this.shouldRefresh = true;
    this.refresh();
  },

  /**
   * Make sure the panel is refreshed (if needed) when it's selected.
   */
  onPanelVisibilityChange: function() {
    this.refresh();
  },

  // Helpers

  /**
   * Return true if the DOM panel is currently selected.
   */
  isPanelVisible: function() {
    return this._toolbox.currentToolId === "dom";
  },

  getPrototypeAndProperties: async function(grip) {
    if (!grip.actor) {
      console.error("No actor!", grip);
      throw new Error("Failed to get actor from grip.");
    }

    // Bail out if target doesn't exist (toolbox maybe closed already).
    if (!this.target) {
      return null;
    }

    // Check for a previously stored request for grip.
    let request = this.pendingRequests.get(grip.actor);

    // If no request is in progress create a new one.
    if (!request) {
      const client = new ObjectClient(this.target.client, grip);
      request = client.getPrototypeAndProperties();
      this.pendingRequests.set(grip.actor, request);
    }

    const response = await request;
    this.pendingRequests.delete(grip.actor);

    // Fire an event about not having any pending requests.
    if (!this.pendingRequests.size) {
      this.emit("no-pending-requests");
    }

    return response;
  },

  openLink: function(url) {
    openContentLink(url);
  },

  getRootGrip: async function() {
    // Attach Console. It might involve RDP communication, so wait
    // asynchronously for the result
    const { result } = await this.target.activeConsole.evaluateJSAsync("window");
    return result;
  },

  postContentMessage: function(type, args) {
    const data = {
      type: type,
      args: args,
    };

    const event = new this.panelWin.MessageEvent("devtools/chrome/message", {
      bubbles: true,
      cancelable: true,
      data: data,
    });

    this.panelWin.dispatchEvent(event);
  },

  onContentMessage: function(event) {
    const data = event.data;
    const method = data.type;
    if (typeof this[method] == "function") {
      this[method](data.args);
    }
  },

  getToolbox: function() {
    return this._toolbox;
  },

  get target() {
    return this._toolbox.target;
  },
};

// Helpers

function exportIntoContentScope(win, obj, defineAs) {
  const clone = Cu.createObjectIn(win, {
    defineAs: defineAs,
  });

  const props = Object.getOwnPropertyNames(obj);
  for (let i = 0; i < props.length; i++) {
    const propName = props[i];
    const propValue = obj[propName];
    if (typeof propValue == "function") {
      Cu.exportFunction(propValue, clone, {
        defineAs: propName,
      });
    }
  }
}

// Exports from this module
exports.DomPanel = DomPanel;
