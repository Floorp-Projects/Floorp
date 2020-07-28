/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * Forward `DOMContentLoaded` and `load` events with precise timing
 * of when events happened according to window.performance numbers.
 *
 * @constructor
 * @param BrowsingContextTarget targetActor
 */
function DocumentEventsListener(targetActor) {
  this.targetActor = targetActor;

  EventEmitter.decorate(this);
  this.onWindowReady = this.onWindowReady.bind(this);
  this.onContentLoaded = this.onContentLoaded.bind(this);
  this.onLoad = this.onLoad.bind(this);
}

exports.DocumentEventsListener = DocumentEventsListener;

DocumentEventsListener.prototype = {
  listen() {
    EventEmitter.on(this.targetActor, "window-ready", this.onWindowReady);
    this.onWindowReady({ window: this.targetActor.window, isTopLevel: true });
  },

  onWindowReady({ window, isTopLevel }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const time = window.performance.timing.navigationStart;
    this.emit("dom-loading", time);

    const { readyState } = window.document;
    if (readyState != "interactive" && readyState != "complete") {
      window.addEventListener("DOMContentLoaded", this.onContentLoaded, {
        once: true,
      });
    } else {
      this.onContentLoaded({ target: window.document });
    }
    if (readyState != "complete") {
      window.addEventListener("load", this.onLoad, { once: true });
    } else {
      this.onLoad({ target: window.document });
    }
  },

  onContentLoaded(event) {
    // milliseconds since the UNIX epoch, when the parser finished its work
    // on the main document, that is when its Document.readyState changes to
    // 'interactive' and the corresponding readystatechange event is thrown
    const window = event.target.defaultView;
    const time = window.performance.timing.domInteractive;
    this.emit("dom-interactive", time);
  },

  onLoad(event) {
    // milliseconds since the UNIX epoch, when the parser finished its work
    // on the main document, that is when its Document.readyState changes to
    // 'complete' and the corresponding readystatechange event is thrown
    const window = event.target.defaultView;
    const time = window.performance.timing.domComplete;
    this.emit("dom-complete", time);
  },

  destroy() {
    this.listener = null;
  },
};
