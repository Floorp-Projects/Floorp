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
 *   need a to observe something on the tabActor's windows.
 *
 * - Dedicated observers: There's only one of them for now: ReflowObserver which
 *   listens to reflow events via the docshell,
 *   These dedicated classes are used by the LayoutChangesObserver.
 */

const {Ci} = require("chrome");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const EventEmitter = require("devtools/shared/old-event-emitter");
const {reflowSpec} = require("devtools/shared/specs/reflow");

/**
 * The reflow actor tracks reflows and emits events about them.
 */
exports.ReflowActor = protocol.ActorClassWithSpec(reflowSpec, {
  initialize: function (conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.tabActor = tabActor;
    this._onReflow = this._onReflow.bind(this);
    this.observer = getLayoutChangesObserver(tabActor);
    this._isStarted = false;
  },

  destroy: function () {
    this.stop();
    releaseLayoutChangesObserver(this.tabActor);
    this.observer = null;
    this.tabActor = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Start tracking reflows and sending events to clients about them.
   * This is a oneway method, do not expect a response and it won't return a
   * promise.
   */
  start: function () {
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
  stop: function () {
    if (this._isStarted) {
      this.observer.off("reflows", this._onReflow);
      this._isStarted = false;
    }
  },

  _onReflow: function (event, reflows) {
    if (this._isStarted) {
      this.emit("reflows", reflows);
    }
  }
});

/**
 * Base class for all sorts of observers that need to listen to events on the
 * tabActor's windows.
 * @param {TabActor} tabActor
 * @param {Function} callback Executed everytime the observer observes something
 */
function Observable(tabActor, callback) {
  this.tabActor = tabActor;
  this.callback = callback;

  this._onWindowReady = this._onWindowReady.bind(this);
  this._onWindowDestroyed = this._onWindowDestroyed.bind(this);

  this.tabActor.on("window-ready", this._onWindowReady);
  this.tabActor.on("window-destroyed", this._onWindowDestroyed);
}

Observable.prototype = {
  /**
   * Is the observer currently observing
   */
  isObserving: false,

  /**
   * Stop observing and detroy this observer instance
   */
  destroy: function () {
    if (this.isDestroyed) {
      return;
    }
    this.isDestroyed = true;

    this.stop();

    this.tabActor.off("window-ready", this._onWindowReady);
    this.tabActor.off("window-destroyed", this._onWindowDestroyed);

    this.callback = null;
    this.tabActor = null;
  },

  /**
   * Start observing whatever it is this observer is supposed to observe
   */
  start: function () {
    if (this.isObserving) {
      return;
    }
    this.isObserving = true;

    this._startListeners(this.tabActor.windows);
  },

  /**
   * Stop observing
   */
  stop: function () {
    if (!this.isObserving) {
      return;
    }
    this.isObserving = false;

    if (this.tabActor.attached && this.tabActor.docShell) {
      // It's only worth stopping if the tabActor is still attached
      this._stopListeners(this.tabActor.windows);
    }
  },

  _onWindowReady: function ({window}) {
    if (this.isObserving) {
      this._startListeners([window]);
    }
  },

  _onWindowDestroyed: function ({window}) {
    if (this.isObserving) {
      this._stopListeners([window]);
    }
  },

  _startListeners: function (windows) {
    // To be implemented by sub-classes.
  },

  _stopListeners: function (windows) {
    // To be implemented by sub-classes.
  },

  /**
   * To be called by sub-classes when something has been observed
   */
  notifyCallback: function (...args) {
    this.isObserving && this.callback && this.callback.apply(null, args);
  }
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
exports.setIgnoreLayoutChanges = function (ignore, syncReflowNode) {
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
 * Use `getLayoutChangesObserver(tabActor)`
 * and `releaseLayoutChangesObserver(tabActor)`
 * which are exported to get and release instances.
 *
 * The observer loops every EVENT_BATCHING_DELAY ms and checks if layout changes
 * have happened since the last loop iteration. If there are, it sends the
 * corresponding events:
 *
 * - "reflows", with an array of all the reflows that occured,
 * - "resizes", with an array of all the resizes that occured,
 *
 * @param {TabActor} tabActor
 */
function LayoutChangesObserver(tabActor) {
  this.tabActor = tabActor;

  this._startEventLoop = this._startEventLoop.bind(this);
  this._onReflow = this._onReflow.bind(this);
  this._onResize = this._onResize.bind(this);

  // Creating the various observers we're going to need
  // For now, just the reflow observer, but later we can add markupMutation,
  // styleSheetChanges and styleRuleChanges
  this.reflowObserver = new ReflowObserver(this.tabActor, this._onReflow);
  this.resizeObserver = new WindowResizeObserver(this.tabActor, this._onResize);

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
  destroy: function () {
    this.isObserving = false;

    this.reflowObserver.destroy();
    this.reflows = null;

    this.resizeObserver.destroy();
    this.hasResized = false;

    this.tabActor = null;
  },

  start: function () {
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

  stop: function () {
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
  _startEventLoop: function () {
    // Avoid emitting events if the tabActor has been detached (may happen
    // during shutdown)
    if (!this.tabActor || !this.tabActor.attached) {
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

    this.eventLoopTimer = this._setTimeout(this._startEventLoop,
      this.EVENT_BATCHING_DELAY);
  },

  _stopEventLoop: function () {
    this._clearTimeout(this.eventLoopTimer);
  },

  // Exposing set/clearTimeout here to let tests override them if needed
  _setTimeout: function (cb, ms) {
    return setTimeout(cb, ms);
  },
  _clearTimeout: function (t) {
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
  _onReflow: function (start, end, isInterruptible) {
    if (gIgnoreLayoutChanges) {
      return;
    }

    // XXX: when/if bug 997092 gets fixed, we will be able to know which
    // elements have been reflowed, which would be a nice thing to add here.
    this.reflows.push({
      start: start,
      end: end,
      isInterruptible: isInterruptible
    });
  },

  /**
   * Executed whenever a resize is observed. Only store a flag saying that a
   * resize occured.
   * The EVENT_BATCHING_DELAY loop will take care of it later.
   */
  _onResize: function () {
    if (gIgnoreLayoutChanges) {
      return;
    }

    this.hasResized = true;
  }
};

/**
 * Get a LayoutChangesObserver instance for a given window. This function makes
 * sure there is only one instance per window.
 * @param {TabActor} tabActor
 * @return {LayoutChangesObserver}
 */
var observedWindows = new Map();
function getLayoutChangesObserver(tabActor) {
  let observerData = observedWindows.get(tabActor);
  if (observerData) {
    observerData.refCounting ++;
    return observerData.observer;
  }

  let obs = new LayoutChangesObserver(tabActor);
  observedWindows.set(tabActor, {
    observer: obs,
    // counting references allows to stop the observer when no tabActor owns an
    // instance.
    refCounting: 1
  });
  obs.start();
  return obs;
}
exports.getLayoutChangesObserver = getLayoutChangesObserver;

/**
 * Release a LayoutChangesObserver instance that was retrieved by
 * getLayoutChangesObserver. This is required to ensure the tabActor reference
 * is removed and the observer is eventually stopped and destroyed.
 * @param {TabActor} tabActor
 */
function releaseLayoutChangesObserver(tabActor) {
  let observerData = observedWindows.get(tabActor);
  if (!observerData) {
    return;
  }

  observerData.refCounting --;
  if (!observerData.refCounting) {
    observerData.observer.destroy();
    observedWindows.delete(tabActor);
  }
}
exports.releaseLayoutChangesObserver = releaseLayoutChangesObserver;

/**
 * Reports any reflow that occurs in the tabActor's docshells.
 * @extends Observable
 * @param {TabActor} tabActor
 * @param {Function} callback Executed everytime a reflow occurs
 */
class ReflowObserver extends Observable {
  constructor(tabActor, callback) {
    super(tabActor, callback);
  }

  _startListeners(windows) {
    for (let window of windows) {
      let docshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .QueryInterface(Ci.nsIDocShell);
      docshell.addWeakReflowObserver(this);
    }
  }

  _stopListeners(windows) {
    for (let window of windows) {
      try {
        let docshell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebNavigation)
                       .QueryInterface(Ci.nsIDocShell);
        docshell.removeWeakReflowObserver(this);
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

ReflowObserver.prototype.QueryInterface = XPCOMUtils
  .generateQI([Ci.nsIReflowObserver, Ci.nsISupportsWeakReference]);

/**
 * Reports window resize events on the tabActor's windows.
 * @extends Observable
 * @param {TabActor} tabActor
 * @param {Function} callback Executed everytime a resize occurs
 */
class WindowResizeObserver extends Observable {

  constructor(tabActor, callback) {
    super(tabActor, callback);
    this.onResize = this.onResize.bind(this);
  }

  _startListeners() {
    this.listenerTarget.addEventListener("resize", this.onResize);
  }

  _stopListeners() {
    this.listenerTarget.removeEventListener("resize", this.onResize);
  }

  onResize() {
    this.notifyCallback();
  }

  get listenerTarget() {
    // For the rootActor, return its window.
    if (this.tabActor.isRootActor) {
      return this.tabActor.window;
    }

    // Otherwise, get the tabActor's chromeEventHandler.
    return this.tabActor.window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsIDocShell)
                               .chromeEventHandler;
  }
}
