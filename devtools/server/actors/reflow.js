/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * About the types of objects in this file:
 *
 * - ReflowActor: the actor class used for protocol purposes.
 *   Mostly empty, just gets an instance of LayoutChangesObserver and forwards
 *   its "reflows" events to clients.
 *
 * - LayoutChangesObserver: extends Observable and uses the ReflowObserver, to
 *   track reflows on the page.
 *   Used by the LayoutActor, but is also exported on the module, so can be used
 *   by any other actor that needs it.
 *
 * - Observable: A utility parent class, meant at being extended by classes that
 *   need a to observe something on the targetActor's windows.
 *
 * - Dedicated observers: There's only one of them for now: ReflowObserver which
 *   listens to reflow events via the docshell,
 *   These dedicated classes are used by the LayoutChangesObserver.
 */

const protocol = require("devtools/shared/protocol");
const EventEmitter = require("devtools/shared/event-emitter");
const { reflowSpec } = require("devtools/shared/specs/reflow");

/**
 * The reflow actor tracks reflows and emits events about them.
 */
exports.ReflowActor = protocol.ActorClassWithSpec(reflowSpec, {
  initialize(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.targetActor = targetActor;
    this._onReflow = this._onReflow.bind(this);
    this.observer = getLayoutChangesObserver(targetActor);
    this._isStarted = false;
  },

  destroy() {
    this.stop();
    releaseLayoutChangesObserver(this.targetActor);
    this.observer = null;
    this.targetActor = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Start tracking reflows and sending events to clients about them.
   * This is a oneway method, do not expect a response and it won't return a
   * promise.
   */
  start() {
    if (!this._isStarted) {
      this.observer.on("reflows", this._onReflow);
      this._isStarted = true;
    }
  },

  /**
   * Stop tracking reflows and sending events to clients about them.
   * This is a oneway method, do not expect a response and it won't return a
   * promise.
   */
  stop() {
    if (this._isStarted) {
      this.observer.off("reflows", this._onReflow);
      this._isStarted = false;
    }
  },

  _onReflow(reflows) {
    if (this._isStarted) {
      this.emit("reflows", reflows);
    }
  },
});

/**
 * Base class for all sorts of observers that need to listen to events on the
 * targetActor's windows.
 * @param {WindowGlobalTargetActor} targetActor
 * @param {Function} callback Executed everytime the observer observes something
 */
function Observable(targetActor, callback) {
  this.targetActor = targetActor;
  this.callback = callback;

  this._onWindowReady = this._onWindowReady.bind(this);
  this._onWindowDestroyed = this._onWindowDestroyed.bind(this);

  this.targetActor.on("window-ready", this._onWindowReady);
  this.targetActor.on("window-destroyed", this._onWindowDestroyed);
}

Observable.prototype = {
  /**
   * Is the observer currently observing
   */
  isObserving: false,

  /**
   * Stop observing and detroy this observer instance
   */
  destroy() {
    if (this.isDestroyed) {
      return;
    }
    this.isDestroyed = true;

    this.stop();

    this.targetActor.off("window-ready", this._onWindowReady);
    this.targetActor.off("window-destroyed", this._onWindowDestroyed);

    this.callback = null;
    this.targetActor = null;
  },

  /**
   * Start observing whatever it is this observer is supposed to observe
   */
  start() {
    if (this.isObserving) {
      return;
    }
    this.isObserving = true;

    this._startListeners(this.targetActor.windows);
  },

  /**
   * Stop observing
   */
  stop() {
    if (!this.isObserving) {
      return;
    }
    this.isObserving = false;

    if (!this.targetActor.isDestroyed() && this.targetActor.docShell) {
      // It's only worth stopping if the targetActor is still active
      this._stopListeners(this.targetActor.windows);
    }
  },

  _onWindowReady({ window }) {
    if (this.isObserving) {
      this._startListeners([window]);
    }
  },

  _onWindowDestroyed({ window }) {
    if (this.isObserving) {
      this._stopListeners([window]);
    }
  },

  _startListeners(windows) {
    // To be implemented by sub-classes.
  },

  _stopListeners(windows) {
    // To be implemented by sub-classes.
  },

  /**
   * To be called by sub-classes when something has been observed
   */
  notifyCallback(...args) {
    this.isObserving && this.callback && this.callback.apply(null, args);
  },
};

/**
 * The LayouChangesObserver will observe reflows as soon as it is started.
 * Some devtools actors may cause reflows and it may be wanted to "hide" these
 * reflows from the LayouChangesObserver consumers.
 * If this is the case, such actors should require this module and use this
 * global function to turn the ignore mode on and off temporarily.
 *
 * Note that if a node is provided, it will be used to force a sync reflow to
 * make sure all reflows which occurred before switching the mode on or off are
 * either observed or ignored depending on the current mode.
 *
 * @param {Boolean} ignore
 * @param {DOMNode} syncReflowNode The node to use to force a sync reflow
 */
var gIgnoreLayoutChanges = false;
exports.setIgnoreLayoutChanges = function(ignore, syncReflowNode) {
  if (syncReflowNode) {
    let forceSyncReflow = syncReflowNode.offsetWidth; // eslint-disable-line
  }
  gIgnoreLayoutChanges = ignore;
};

/**
 * The LayoutChangesObserver class is instantiated only once per given tab
 * and is used to track reflows and dom and style changes in that tab.
 * The LayoutActor uses this class to send reflow events to its clients.
 *
 * This class isn't exported on the module because it shouldn't be instantiated
 * to avoid creating several instances per tabs.
 * Use `getLayoutChangesObserver(targetActor)`
 * and `releaseLayoutChangesObserver(targetActor)`
 * which are exported to get and release instances.
 *
 * The observer loops every EVENT_BATCHING_DELAY ms and checks if layout changes
 * have happened since the last loop iteration. If there are, it sends the
 * corresponding events:
 *
 * - "reflows", with an array of all the reflows that occured,
 * - "resizes", with an array of all the resizes that occured,
 *
 * @param {WindowGlobalTargetActor} targetActor
 */
function LayoutChangesObserver(targetActor) {
  this.targetActor = targetActor;

  this._startEventLoop = this._startEventLoop.bind(this);
  this._onReflow = this._onReflow.bind(this);
  this._onResize = this._onResize.bind(this);

  // Creating the various observers we're going to need
  // For now, just the reflow observer, but later we can add markupMutation,
  // styleSheetChanges and styleRuleChanges
  this.reflowObserver = new ReflowObserver(this.targetActor, this._onReflow);
  this.resizeObserver = new WindowResizeObserver(
    this.targetActor,
    this._onResize
  );

  EventEmitter.decorate(this);
}

exports.LayoutChangesObserver = LayoutChangesObserver;

LayoutChangesObserver.prototype = {
  /**
   * How long does this observer waits before emitting batched events.
   * The lower the value, the more event packets will be sent to clients,
   * potentially impacting performance.
   * The higher the value, the more time we'll wait, this is better for
   * performance but has an effect on how soon changes are shown in the toolbox.
   */
  EVENT_BATCHING_DELAY: 300,

  /**
   * Destroying this instance of LayoutChangesObserver will stop the batched
   * events from being sent.
   */
  destroy() {
    this.isObserving = false;

    this.reflowObserver.destroy();
    this.reflows = null;

    this.resizeObserver.destroy();
    this.hasResized = false;

    this.targetActor = null;
  },

  start() {
    if (this.isObserving) {
      return;
    }
    this.isObserving = true;

    this.reflows = [];
    this.hasResized = false;

    this._startEventLoop();

    this.reflowObserver.start();
    this.resizeObserver.start();
  },

  stop() {
    if (!this.isObserving) {
      return;
    }
    this.isObserving = false;

    this._stopEventLoop();

    this.reflows = [];
    this.hasResized = false;

    this.reflowObserver.stop();
    this.resizeObserver.stop();
  },

  /**
   * Start the event loop, which regularly checks if there are any observer
   * events to be sent as batched events
   * Calls itself in a loop.
   */
  _startEventLoop() {
    // Avoid emitting events if the targetActor has been detached (may happen
    // during shutdown)
    if (!this.targetActor || this.targetActor.isDestroyed()) {
      return;
    }

    // Send any reflows we have
    if (this.reflows && this.reflows.length) {
      this.emit("reflows", this.reflows);
      this.reflows = [];
    }

    // Send any resizes we have
    if (this.hasResized) {
      this.emit("resize");
      this.hasResized = false;
    }

    this.eventLoopTimer = this._setTimeout(
      this._startEventLoop,
      this.EVENT_BATCHING_DELAY
    );
  },

  _stopEventLoop() {
    this._clearTimeout(this.eventLoopTimer);
  },

  // Exposing set/clearTimeout here to let tests override them if needed
  _setTimeout(cb, ms) {
    return setTimeout(cb, ms);
  },
  _clearTimeout(t) {
    return clearTimeout(t);
  },

  /**
   * Executed whenever a reflow is observed. Only stacks the reflow in the
   * reflows array.
   * The EVENT_BATCHING_DELAY loop will take care of it later.
   * @param {Number} start When the reflow started
   * @param {Number} end When the reflow ended
   * @param {Boolean} isInterruptible
   */
  _onReflow(start, end, isInterruptible) {
    if (gIgnoreLayoutChanges) {
      return;
    }

    // XXX: when/if bug 997092 gets fixed, we will be able to know which
    // elements have been reflowed, which would be a nice thing to add here.
    this.reflows.push({
      start,
      end,
      isInterruptible,
    });
  },

  /**
   * Executed whenever a resize is observed. Only store a flag saying that a
   * resize occured.
   * The EVENT_BATCHING_DELAY loop will take care of it later.
   */
  _onResize() {
    if (gIgnoreLayoutChanges) {
      return;
    }

    this.hasResized = true;
  },
};

/**
 * Get a LayoutChangesObserver instance for a given window. This function makes
 * sure there is only one instance per window.
 * @param {WindowGlobalTargetActor} targetActor
 * @return {LayoutChangesObserver}
 */
var observedWindows = new Map();
function getLayoutChangesObserver(targetActor) {
  const observerData = observedWindows.get(targetActor);
  if (observerData) {
    observerData.refCounting++;
    return observerData.observer;
  }

  const obs = new LayoutChangesObserver(targetActor);
  observedWindows.set(targetActor, {
    observer: obs,
    // counting references allows to stop the observer when no targetActor owns an
    // instance.
    refCounting: 1,
  });
  obs.start();
  return obs;
}
exports.getLayoutChangesObserver = getLayoutChangesObserver;

/**
 * Release a LayoutChangesObserver instance that was retrieved by
 * getLayoutChangesObserver. This is required to ensure the targetActor reference
 * is removed and the observer is eventually stopped and destroyed.
 * @param {WindowGlobalTargetActor} targetActor
 */
function releaseLayoutChangesObserver(targetActor) {
  const observerData = observedWindows.get(targetActor);
  if (!observerData) {
    return;
  }

  observerData.refCounting--;
  if (!observerData.refCounting) {
    observerData.observer.destroy();
    observedWindows.delete(targetActor);
  }
}
exports.releaseLayoutChangesObserver = releaseLayoutChangesObserver;

/**
 * Reports any reflow that occurs in the targetActor's docshells.
 * @extends Observable
 * @param {WindowGlobalTargetActor} targetActor
 * @param {Function} callback Executed everytime a reflow occurs
 */
class ReflowObserver extends Observable {
  constructor(targetActor, callback) {
    super(targetActor, callback);
  }

  _startListeners(windows) {
    for (const window of windows) {
      window.docShell.addWeakReflowObserver(this);
    }
  }

  _stopListeners(windows) {
    for (const window of windows) {
      try {
        window.docShell.removeWeakReflowObserver(this);
      } catch (e) {
        // Corner cases where a global has already been freed may happen, in
        // which case, no need to remove the observer.
      }
    }
  }

  reflow(start, end) {
    this.notifyCallback(start, end, false);
  }

  reflowInterruptible(start, end) {
    this.notifyCallback(start, end, true);
  }
}

ReflowObserver.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIReflowObserver",
  "nsISupportsWeakReference",
]);

/**
 * Reports window resize events on the targetActor's windows.
 * @extends Observable
 * @param {WindowGlobalTargetActor} targetActor
 * @param {Function} callback Executed everytime a resize occurs
 */
class WindowResizeObserver extends Observable {
  constructor(targetActor, callback) {
    super(targetActor, callback);

    this.onNavigate = this.onNavigate.bind(this);
    this.onResize = this.onResize.bind(this);

    this.targetActor.on("navigate", this.onNavigate);
  }

  _startListeners() {
    this.listenerTarget.addEventListener("resize", this.onResize);
  }

  _stopListeners() {
    this.listenerTarget.removeEventListener("resize", this.onResize);
  }

  onNavigate() {
    if (this.isObserving) {
      this._stopListeners();
      this._startListeners();
    }
  }

  onResize() {
    this.notifyCallback();
  }

  destroy() {
    if (this.targetActor) {
      this.targetActor.off("navigate", this.onNavigate);
    }
    super.destroy();
  }

  get listenerTarget() {
    // For the rootActor, return its window.
    if (this.targetActor.isRootActor) {
      return this.targetActor.window;
    }

    // Otherwise, get the targetActor's chromeEventHandler.
    return this.targetActor.chromeEventHandler;
  }
}
