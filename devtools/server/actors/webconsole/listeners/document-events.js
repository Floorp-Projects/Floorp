/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global XPCNativeWrapper */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");

/**
 * About "navigationStart - ${WILL_NAVIGATE_TIME_SHIFT}ms":
 * Unfortunately dom-loading's navigationStart timestamp is older than the navigationStart we receive from will-navigate.
 *
 * That's because we record `navigationStart` before will-navigate code is called.
 * And will-navigate code don't have access to performance.timing.navigationStart that dom-loading is using.
 * The `performance.timing.navigationStart` is recorded earlier from `DocumentLoadListener.SetNavigating`, here:
 *   https://searchfox.org/mozilla-central/rev/9b430bb1a11d7152cab2af4574f451ffb906b052/netwerk/ipc/DocumentLoadListener.cpp#907-908
 *   https://searchfox.org/mozilla-central/rev/9b430bb1a11d7152cab2af4574f451ffb906b052/netwerk/ipc/DocumentLoadListener.cpp#820-823
 * While this function is being called via `nsIWebProgressListener.onStateChange`, here:
 *   https://searchfox.org/mozilla-central/rev/9b430bb1a11d7152cab2af4574f451ffb906b052/netwerk/ipc/DocumentLoadListener.cpp#934-939
 * And we record the navigationStart timestamp from onStateChange by using Date.now(), which is more recent
 * than performance.timing.navigationStart.
 *
 * We do this workaround because all DOCUMENT_EVENT comes with a "time" timestamp.
 * Each event relates to a particular event in the lifecycle of documents and are supposed to follow a particular order:
 *  - will-navigate (on the previous target)
 *  - dom-loading (on the new target)
 *  - dom-interactive
 *  - dom-complete
 * And some tests are asserting this.
 */
const WILL_NAVIGATE_TIME_SHIFT = 20;
exports.WILL_NAVIGATE_TIME_SHIFT = WILL_NAVIGATE_TIME_SHIFT;

/**
 * Forward `DOMContentLoaded` and `load` events with precise timing
 * of when events happened according to window.performance numbers.
 *
 * @constructor
 * @param WindowGlobalTarget targetActor
 */
function DocumentEventsListener(targetActor) {
  this.targetActor = targetActor;

  EventEmitter.decorate(this);
  this.onWillNavigate = this.onWillNavigate.bind(this);
  this.onWindowReady = this.onWindowReady.bind(this);
  this.onContentLoaded = this.onContentLoaded.bind(this);
  this.onLoad = this.onLoad.bind(this);
}

exports.DocumentEventsListener = DocumentEventsListener;

DocumentEventsListener.prototype = {
  listen() {
    // Listen to will-navigate and do not emit a fake one as we only care about upcoming navigation
    this.targetActor.on("will-navigate", this.onWillNavigate);

    // Listen to window-ready and then fake one in order to notify about dom-loading for the existing document
    this.targetActor.on("window-ready", this.onWindowReady);
    // The target actor already emitted a window-ready for the top document when instantiating.
    // So fake one for the top document right away.
    this.onWindowReady({
      window: this.targetActor.window,
      isTopLevel: true,
    });
  },

  onWillNavigate({
    window,
    isTopLevel,
    newURI,
    navigationStart,
    isFrameSwitching,
  }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    this.emit("will-navigate", {
      time: navigationStart - WILL_NAVIGATE_TIME_SHIFT,
      newURI,
      isFrameSwitching,
    });
  },

  onWindowReady({ window, isTopLevel, isFrameSwitching }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const time = window.performance.timing.navigationStart;

    this.emit("dom-loading", {
      time,
      isFrameSwitching,
    });

    const { readyState } = window.document;
    if (readyState != "interactive" && readyState != "complete") {
      window.addEventListener(
        "DOMContentLoaded",
        e => this.onContentLoaded(e, isFrameSwitching),
        {
          once: true,
        }
      );
    } else {
      this.onContentLoaded({ target: window.document }, isFrameSwitching);
    }
    if (readyState != "complete") {
      window.addEventListener("load", e => this.onLoad(e, isFrameSwitching), {
        once: true,
      });
    } else {
      this.onLoad({ target: window.document }, isFrameSwitching);
    }
  },

  onContentLoaded(event, isFrameSwitching) {
    if (this.destroyed) {
      return;
    }
    // milliseconds since the UNIX epoch, when the parser finished its work
    // on the main document, that is when its Document.readyState changes to
    // 'interactive' and the corresponding readystatechange event is thrown
    const window = event.target.defaultView;
    const time = window.performance.timing.domInteractive;
    this.emit("dom-interactive", { time, isFrameSwitching });
  },

  onLoad(event, isFrameSwitching) {
    if (this.destroyed) {
      return;
    }
    // milliseconds since the UNIX epoch, when the parser finished its work
    // on the main document, that is when its Document.readyState changes to
    // 'complete' and the corresponding readystatechange event is thrown
    const window = event.target.defaultView;
    const time = window.performance.timing.domComplete;
    this.emit("dom-complete", {
      time,
      isFrameSwitching,
      hasNativeConsoleAPI: this.hasNativeConsoleAPI(window),
    });
  },

  /**
   * Tells if the window.console object is native or overwritten by script in
   * the page.
   *
   * @param nsIDOMWindow window
   *        The window object you want to check.
   * @return boolean
   *         True if the window.console object is native, or false otherwise.
   */
  hasNativeConsoleAPI(window) {
    let isNative = false;
    try {
      // We are very explicitly examining the "console" property of
      // the non-Xrayed object here.
      const console = window.wrappedJSObject.console;
      // In xpcshell tests, console ends up being undefined and XPCNativeWrapper
      // crashes in debug builds.
      if (console) {
        isNative = new XPCNativeWrapper(console).IS_NATIVE_CONSOLE === true;
      }
    } catch (ex) {
      // ignore
    }
    return isNative;
  },

  destroy() {
    // Also use a flag to silent onContentLoad and onLoad events
    this.destroyed = true;
    this.targetActor.off("will-navigate", this.onWillNavigate);
    this.targetActor.off("window-ready", this.onWindowReady);
  },
};
