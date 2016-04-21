/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const { defer } = require("sdk/core/promise");
const { ObjectClient } = require("devtools/shared/client/main");

const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");

/**
 * This object represents DOM panel. It's responsibility is to
 * render Document Object Model of the current debugger target.
 */
function DomPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;

  this.onTabNavigated = this.onTabNavigated.bind(this);
  this.onContentMessage = this.onContentMessage.bind(this);

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
  open: Task.async(function*() {
    if (this._opening) {
      return this._opening;
    }

    let deferred = promise.defer();
    this._opening = deferred.promise;

    // Local monitoring needs to make the target remote.
    if (!this.target.isRemote) {
      yield this.target.makeRemote();
    }

    this.initialize();

    this.once("no-pending-requests", () => {
      this.isReady = true;
      this.emit("ready");
      deferred.resolve(this);
    });

    return this._opening;
  }),

  // Initialization

  initialize: function() {
    this.panelWin.addEventListener("devtools/content/message",
      this.onContentMessage, true);

    this.target.on("navigate", this.onTabNavigated);

    let provider = {
      getPrototypeAndProperties: this.getPrototypeAndProperties.bind(this)
    };

    exportIntoContentScope(this.panelWin, provider, "DomProvider");

    this.doRefresh();
  },

  destroy: Task.async(function*() {
    if (this._destroying) {
      return this._destroying;
    }

    let deferred = promise.defer();
    this._destroying = deferred.promise;

    this.target.off("navigate", this.onTabNavigated);

    this.emit("destroyed");

    deferred.resolve();
    return this._destroying;
  }),

  // Events

  doRefresh: function() {
    this.refresh().then(rootGrip => {
      this.postContentMessage("initialize", rootGrip);
    });
  },

  onTabNavigated: function() {
    this.doRefresh();
  },

  getPrototypeAndProperties: function(grip) {
    let deferred = defer();

    if (!grip.actor) {
      console.error("No actor!", grip);
      deferred.reject(new Error("Failed to get actor from grip."));
      return deferred.promise;
    }

    // Bail out if target doesn't exist (toolbox maybe closed already).
    if (!this.target) {
      return deferred.promise;
    }

    // If a request for the grips is already in progress
    // use the same promise.
    let request = this.pendingRequests.get(grip.actor);
    if (request) {
      return request;
    }

    let client = new ObjectClient(this.target.client, grip);
    client.getPrototypeAndProperties(response => {
      this.pendingRequests.delete(grip.actor, deferred.promise);
      deferred.resolve(response);

      // Fire an event about not having any pending requests.
      if (!this.pendingRequests.size) {
        this.emit("no-pending-requests");
      }
    });

    this.pendingRequests.set(grip.actor, deferred.promise);

    return deferred.promise;
  },

  // Refresh

  refresh: function() {
    let deferred = defer();

    // Attach Console. It might involve RDP communication, so wait
    // asynchronously for the result
    this.target.activeConsole.evaluateJSAsync("window", res => {
      deferred.resolve(res.result);
    });

    return deferred.promise;
  },

  // Helpers

  postContentMessage: function(type, args) {
    let data = {
      type: type,
      args: args,
    };

    let event = new this.panelWin.MessageEvent("devtools/chrome/message", {
      bubbles: true,
      cancelable: true,
      data: data,
    });

    this.panelWin.dispatchEvent(event);
  },

  onContentMessage: function(event) {
    let data = event.data;
    let method = data.type;
    if (typeof this[method] == "function") {
      this[method](data.args);
    }
  },

  get target() {
    return this._toolbox.target;
  },
};

// Helpers

function exportIntoContentScope(win, obj, defineAs) {
  let clone = Cu.createObjectIn(win, {
    defineAs: defineAs
  });

  let props = Object.getOwnPropertyNames(obj);
  for (let i = 0; i < props.length; i++) {
    let propName = props[i];
    let propValue = obj[propName];
    if (typeof propValue == "function") {
      Cu.exportFunction(propValue, clone, {
        defineAs: propName
      });
    }
  }
}

// Exports from this module
exports.DomPanel = DomPanel;
