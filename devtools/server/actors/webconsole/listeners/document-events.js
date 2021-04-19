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
    // If the target actor isn't attached yet, attach it so that it starts emitting window-ready event
    if (!this.targetActor.attached) {
      // The target actor will emit a window-ready in the next event loop
      // for the top level document (and any existing iframe document)
      this.targetActor.attach();
    } else {
      // If the target is already attached, it already emitted in the past a window-ready for the top document.
      // So fake one for the top document right away.
      this.onWindowReady({
        window: this.targetActor.window,
        isTopLevel: true,
        // Flag the very first dom-loading event, which is about the top target and may come
        // after some other already existing resources.
        shouldBeIgnoredAsRedundantWithTargetAvailable: true,
      });
    }
  },

  onWindowReady({
    window,
    isTopLevel,
    shouldBeIgnoredAsRedundantWithTargetAvailable,
  }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const time = window.performance.timing.navigationStart;
    this.emit(
      "dom-loading",
      time,
      // As dom-loading is often used to clear the panel on navigation, and is typically
      // sent before any other resource, we need to add a hint so the client knows when
      // then event can be ignored.
      // We should also ignore them if the Target was created via a JSWindowActor and is
      // destroyed when the WindowGlobal is destroyed (i.e. when we navigate or reload),
      // as this will come late and is redundant with onTargetAvailable.
      shouldBeIgnoredAsRedundantWithTargetAvailable ||
        (this.targetActor.isTopLevelTarget &&
          this.targetActor.followWindowGlobalLifeCycle)
    );

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
