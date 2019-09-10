/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const xpcInspector = require("xpcInspector");
const { Cu } = require("chrome");

/**
 * Manages pushing event loops and automatically pops and exits them in the
 * correct order as they are resolved.
 *
 * @param ThreadActor thread
 *        The thread actor instance that owns this EventLoopStack.
 */
function EventLoopStack({ thread }) {
  this._thread = thread;
}

EventLoopStack.prototype = {
  /**
   * The number of nested event loops on the stack.
   */
  get size() {
    return xpcInspector.eventLoopNestLevel;
  },

  /**
   * The thread actor of the debuggee who pushed the event loop on top of the stack.
   */
  get lastPausedThreadActor() {
    if (this.size > 0) {
      return xpcInspector.lastNestRequestor.thread;
    }
    return null;
  },

  /**
   * Push a new nested event loop onto the stack.
   *
   * @returns EventLoop
   */
  push: function() {
    return new EventLoop({
      thread: this._thread,
    });
  },
};

/**
 * An object that represents a nested event loop. It is used as the nest
 * requestor with nsIJSInspector instances.
 *
 * @param ThreadActor thread
 *        The thread actor that is creating this nested event loop.
 */
function EventLoop({ thread }) {
  this._thread = thread;

  this.enter = this.enter.bind(this);
  this.resolve = this.resolve.bind(this);
}

EventLoop.prototype = {
  entered: false,
  resolved: false,
  get thread() {
    return this._thread;
  },

  /**
   * Enter this nested event loop.
   */
  enter: function() {
    const preNestData = this.preNest();

    this.entered = true;
    xpcInspector.enterNestedEventLoop(this);

    // Keep exiting nested event loops while the last requestor is resolved.
    if (xpcInspector.eventLoopNestLevel > 0) {
      const { resolved } = xpcInspector.lastNestRequestor;
      if (resolved) {
        xpcInspector.exitNestedEventLoop();
      }
    }

    this.postNest(preNestData);
  },

  /**
   * Resolve this nested event loop.
   *
   * @returns boolean
   *          True if we exited this nested event loop because it was on top of
   *          the stack, false if there is another nested event loop above this
   *          one that hasn't resolved yet.
   */
  resolve: function() {
    if (!this.entered) {
      throw new Error(
        "Can't resolve an event loop before it has been entered!"
      );
    }
    if (this.resolved) {
      throw new Error("Already resolved this nested event loop!");
    }
    this.resolved = true;
    if (this === xpcInspector.lastNestRequestor) {
      xpcInspector.exitNestedEventLoop();
      return true;
    }
    return false;
  },

  /**
   * Retrieve the list of all DOM Windows debugged by the current thread actor.
   */
  getAllWindowDebuggees() {
    return (
      this._thread.dbg
        .getDebuggees()
        .filter(debuggee => {
          // Select only debuggee that relates to windows
          // e.g. ignore sandboxes, jsm and such
          return debuggee.class == "Window";
        })
        .map(debuggee => {
          // Retrieve the JS reference for these windows
          return debuggee.unsafeDereference();
        })
        // Ignore iframes as they will be paused automatically when pausing their
        // owner top level document
        .filter(window => !Cu.isDeadWrapper(window) && window.top === window)
    );
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest() {
    const windows = [];
    // Disable events in all open windows.
    for (const window of this.getAllWindowDebuggees()) {
      const { windowUtils } = window;
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
      windows.push(window);
    }
    return windows;
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest(pausedWindows) {
    // Enable events in all open windows.
    for (const window of pausedWindows) {
      const { windowUtils } = window;
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  },
};

exports.EventLoopStack = EventLoopStack;
