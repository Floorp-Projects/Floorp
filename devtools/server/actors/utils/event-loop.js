/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const xpcInspector = require("xpcInspector");

/**
 * An object that represents a nested event loop. It is used as the nest
 * requestor with nsIJSInspector instances.
 *
 * @param ThreadActor thread
 *        The thread actor that is creating this nested event loop.
 */
class EventLoop {
  constructor({ thread }) {
    this._thread = thread;

    // A flag which is true in between the two calls to enter() and exit().
    this._entered = false;
    // Another flag which is true only after having called exit().
    // Note that this EventLoop may still be paused and its enter() method
    // still be on hold, if another EventLoop paused about this one.
    this._resolved = false;
  }

  /**
   * This is meant for other thread actors, and is used by other thread actor's
   * EventLoop's isTheLastPausedThreadActor()
   */
  get thread() {
    return this._thread;
  }
  /**
   * Similarly, it will be used by another thread actor's EventLoop's enter() method
   */
  get resolved() {
    return this._resolved;
  }

  /**
   * Tells if the last thread actor to have paused (i.e. last EventLoop on the stack)
   * is the current one.
   *
   * We avoid trying to exit this event loop,
   * if another thread actor pile up a more recent one.
   * All the event loops will be effectively exited when
   * the thread actor which piled up the most recent nested event loop resumes.
   *
   * For convenience for the callsite, this will return true if nothing paused.
   */
  isTheLastPausedThreadActor() {
    if (xpcInspector.eventLoopNestLevel > 0) {
      return xpcInspector.lastNestRequestor.thread === this._thread;
    }
    return true;
  }

  /**
   * Enter a new nested event loop.
   */
  enter() {
    if (this._entered) {
      throw new Error(
        "Can't enter an event loop that has already been entered!"
      );
    }

    const preEnterData = this.preEnter();

    this._entered = true;
    // Note: next line will synchronously block the execution until exit() is being called.
    //
    // This enterNestedEventLoop is a bit magical and will break run-to-completion rule of JS.
    // JS will become multi-threaded. Some other task may start running on change state
    // while we are blocked on this enterNestedEventLoop function call.
    // You may find valuable information about Tasks and Event Loops on:
    // https://docs.google.com/document/d/1jTMd-H_BwH9_QNUDxPse80vq884_hMvd234lvE5gqY8/edit?usp=sharing
    //
    // Note #2: this will update xpcInspector.lastNestRequestor to this
    xpcInspector.enterNestedEventLoop(this);

    // If this code runs, it means that we just exited this event loop and lastNestRequestor is no longer equal to this.
    //
    // We will now "recursively" exit all the resolved EventLoops which are blocked on `enterNestedEventLoop`:
    // - if the new lastNestRequestor is resolved, request to exit it as well
    // - this lastNestRequestor is another EventLoop instance
    // - exiting this EventLoop unblocks its "enter" method and moves lastNestRequestor to the next requestor (if any)
    // - we go back to the first step, and attempt to exit the new lastNestRequestor if it is resolved, etc...
    if (xpcInspector.eventLoopNestLevel > 0) {
      const { resolved } = xpcInspector.lastNestRequestor;
      if (resolved) {
        xpcInspector.exitNestedEventLoop();
      }
    }

    this.postExit(preEnterData);
  }

  /**
   * Exit this nested event loop.
   *
   * @returns boolean
   *          True if we exited this nested event loop because it was on top of
   *          the stack, false if there is another nested event loop above this
   *          one that hasn't exited yet.
   */
  exit() {
    if (!this._entered) {
      throw new Error("Can't exit an event loop before it has been entered!");
    }
    this._entered = false;
    this._resolved = true;

    // If another ThreadActor paused and spawn a new nested event loop after this one,
    // let it resume the thread and ignore this call.
    // The code calling exitNestedEventLoop from EventLoop.enter will resume execution,
    // by seeing that resolved attribute that we just toggled is true.
    //
    // Note that ThreadActor.resume method avoids calling exit thanks to `isTheLastPausedThreadActor`
    // So for all use requests to resume, the ThreadActor won't call exit until it is the last
    // thread actor to have entered a nested EventLoop.
    if (this === xpcInspector.lastNestRequestor) {
      xpcInspector.exitNestedEventLoop();
      return true;
    }
    return false;
  }

  /**
   * Retrieve the list of all DOM Windows debugged by the current thread actor.
   */
  getAllWindowDebuggees() {
    return this._thread.dbg
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

      .filter(window => {
        // Ignore document which have already been nuked,
        // so navigated to another location and removed from memory completely.
        if (Cu.isDeadWrapper(window)) {
          return false;
        }
        // Also ignore document which are closed, as trying to access window.parent or top would throw NS_ERROR_NOT_INITIALIZED
        if (window.closed) {
          return false;
        }
        // Ignore remote iframes, which will be debugged by another thread actor,
        // running in the remote process
        if (Cu.isRemoteProxy(window)) {
          return false;
        }
        // Accept "top remote iframe document":
        // document of iframe whose immediate parent is in another process.
        if (Cu.isRemoteProxy(window.parent) && !Cu.isRemoteProxy(window)) {
          return true;
        }

        // If EFT is enabled, accept any same process document (top-level or iframe).
        if (this.thread.getParent().ignoreSubFrames) {
          return true;
        }

        try {
          // Ignore iframes running in the same process as their parent document,
          // as they will be paused automatically when pausing their owner top level document
          return window.top === window;
        } catch (e) {
          // Warn if this is throwing for an unknown reason, but suppress the
          // exception regardless so that we can enter the nested event loop.
          if (!/not initialized/.test(e)) {
            console.warn(`Exception in getAllWindowDebuggees: ${e}`);
          }
          return false;
        }
      });
  }

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preEnter() {
    const docShells = [];
    // Disable events in all open windows.
    for (const window of this.getAllWindowDebuggees()) {
      const { windowUtils } = window;
      windowUtils.suppressEventHandling(true);
      windowUtils.suspendTimeouts();
      docShells.push(window.docShell);
    }
    return docShells;
  }

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postExit(pausedDocShells) {
    // Enable events in all window paused in preEnter
    for (const docShell of pausedDocShells) {
      // Do not try to resume documents which are in destruction
      // as resume methods would throw
      if (docShell.isBeingDestroyed()) {
        continue;
      }
      const { windowUtils } = docShell.domWindow;
      windowUtils.resumeTimeouts();
      windowUtils.suppressEventHandling(false);
    }
  }
}

exports.EventLoop = EventLoop;
