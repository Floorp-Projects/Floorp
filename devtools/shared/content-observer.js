/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * Handles adding an observer for the creation of content document globals,
 * event sent immediately after a web content document window has been set up,
 * but before any script code has been executed.
 */
function ContentObserver(targetActor) {
  this._contentWindow = targetActor.window;
  this._onContentGlobalCreated = this._onContentGlobalCreated.bind(this);
  this._onInnerWindowDestroyed = this._onInnerWindowDestroyed.bind(this);
  this.startListening();
}

module.exports.ContentObserver = ContentObserver;

ContentObserver.prototype = {
  /**
   * Starts listening for the required observer messages.
   */
  startListening() {
    Services.obs.addObserver(
      this._onContentGlobalCreated,
      "content-document-global-created"
    );
    Services.obs.addObserver(
      this._onInnerWindowDestroyed,
      "inner-window-destroyed"
    );
  },

  /**
   * Stops listening for the required observer messages.
   */
  stopListening() {
    Services.obs.removeObserver(
      this._onContentGlobalCreated,
      "content-document-global-created"
    );
    Services.obs.removeObserver(
      this._onInnerWindowDestroyed,
      "inner-window-destroyed"
    );
  },

  /**
   * Fired immediately after a web content document window has been set up.
   */
  _onContentGlobalCreated(subject, topic, data) {
    if (subject == this._contentWindow) {
      EventEmitter.emit(this, "global-created", subject);
    }
  },

  /**
   * Fired when an inner window is removed from the backward/forward cache.
   */
  _onInnerWindowDestroyed(subject, topic, data) {
    const id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    EventEmitter.emit(this, "global-destroyed", id);
  },
};

// Utility functions.

ContentObserver.GetInnerWindowID = function(window) {
  return window.windowGlobalChild.innerWindowId;
};
