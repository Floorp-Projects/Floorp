/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * Forward `DOMContentLoaded` and `load` events with precise timing
 * of when events happened according to window.performance numbers.
 *
 * @constructor
 * @param object console
 *        The web console actor.
 */
function DocumentEventsListener(console) {
  this.console = console;

  this.onWindowReady = this.onWindowReady.bind(this);
  this.onContentLoaded = this.onContentLoaded.bind(this);
  this.onLoad = this.onLoad.bind(this);
  this.listen();
}

exports.DocumentEventsListener = DocumentEventsListener;

DocumentEventsListener.prototype = {
  listen() {
    EventEmitter.on(
      this.console.parentActor,
      "window-ready",
      this.onWindowReady
    );
    this.onWindowReady({ window: this.console.window, isTopLevel: true });
  },

  onWindowReady({ window, isTopLevel }) {
    // Avoid listening if the console actor is already destroyed
    if (!this.console.conn) {
      return;
    }

    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const packet = {
      from: this.console.actorID,
      type: "documentEvent",
      name: "dom-loading",
      time: window.performance.timing.navigationStart,
    };
    this.console.conn.send(packet);

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
    // Avoid emitting an event when the console actor is already destroyed
    if (!this.console.conn) {
      return;
    }

    const window = event.target.defaultView;
    const packet = {
      from: this.console.actorID,
      type: "documentEvent",
      name: "dom-interactive",
      // milliseconds since the UNIX epoch, when the parser finished its work
      // on the main document, that is when its Document.readyState changes to
      // 'interactive' and the corresponding readystatechange event is thrown
      time: window.performance.timing.domInteractive,
    };
    this.console.conn.send(packet);
  },

  onLoad(event) {
    // Avoid emitting an event when the console actor is already destroyed
    if (!this.console.conn) {
      return;
    }

    const window = event.target.defaultView;
    const packet = {
      from: this.console.actorID,
      type: "documentEvent",
      name: "dom-complete",
      // milliseconds since the UNIX epoch, when the parser finished its work
      // on the main document, that is when its Document.readyState changes to
      // 'complete' and the corresponding readystatechange event is thrown
      time: window.performance.timing.domComplete,
    };
    this.console.conn.send(packet);
  },

  destroy() {
    EventEmitter.off(
      this.console.parentActor,
      "window-ready",
      this.onWindowReady
    );

    this.listener = null;
  },
};
