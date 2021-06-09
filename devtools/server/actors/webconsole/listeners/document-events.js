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
const WILL_NAVIGATE_TIME_SHIFT = 5;
exports.WILL_NAVIGATE_TIME_SHIFT = WILL_NAVIGATE_TIME_SHIFT;

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
  this.onWillNavigate = this.onWillNavigate.bind(this);
  this.onWindowReady = this.onWindowReady.bind(this);
  this.onContentLoaded = this.onContentLoaded.bind(this);
  this.onLoad = this.onLoad.bind(this);
}

exports.DocumentEventsListener = DocumentEventsListener;

DocumentEventsListener.prototype = {
  listen() {
    // Listen to will-navigate and do not emit a fake one as we only care about upcoming navigation
    EventEmitter.on(this.targetActor, "will-navigate", this.onWillNavigate);

    // Listen to window-ready and then fake one in order to notify about dom-loading for the existing document
    EventEmitter.on(this.targetActor, "window-ready", this.onWindowReady);
    // If the target actor isn't attached yet, attach it so that it starts emitting window-ready event
    // Only do that if this isn't a JSWindowActor based target as this won't emit window-ready anyway.
    if (
      !this.targetActor.attached &&
      !this.targetActor.followWindowGlobalLifeCycle
    ) {
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

  onWillNavigate({ window, isTopLevel, newURI, navigationStart }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    this.emit("will-navigate", {
      time: navigationStart - WILL_NAVIGATE_TIME_SHIFT,
      newURI,
    });
  },

  onWindowReady({
    window,
    isTopLevel,
    shouldBeIgnoredAsRedundantWithTargetAvailable,
    isFrameSwitching,
  }) {
    // Ignore iframes
    if (!isTopLevel) {
      return;
    }

    const time = window.performance.timing.navigationStart;

    // As dom-loading is often used to clear the panel on navigation, and is typically
    // sent before any other resource, we need to add a hint so the client knows when
    // then event can be ignored.
    // We should also ignore them if the Target was created via a JSWindowActor and is
    // destroyed when the WindowGlobal is destroyed (i.e. when we navigate or reload),
    // as this will come late and is redundant with onTargetAvailable.
    shouldBeIgnoredAsRedundantWithTargetAvailable =
      shouldBeIgnoredAsRedundantWithTargetAvailable ||
      (this.targetActor.isTopLevelTarget &&
        this.targetActor.followWindowGlobalLifeCycle);

    this.emit("dom-loading", {
      time,
      shouldBeIgnoredAsRedundantWithTargetAvailable,
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
    // milliseconds since the UNIX epoch, when the parser finished its work
    // on the main document, that is when its Document.readyState changes to
    // 'interactive' and the corresponding readystatechange event is thrown
    const window = event.target.defaultView;
    const time = window.performance.timing.domInteractive;
    this.emit("dom-interactive", { time, isFrameSwitching });
  },

  onLoad(event, isFrameSwitching) {
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
    this.listener = null;
  },
};
