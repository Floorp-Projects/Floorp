/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
loader.lazyRequireGetter(
  this,
  "WorkerTargetActor",
  "devtools/server/actors/targets/worker",
  true
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

function matchWorkerDebugger(dbg, options) {
  if ("type" in options && dbg.type !== options.type) {
    return false;
  }
  if ("window" in options) {
    let window = dbg.window;
    while (window !== null && window.parent !== window) {
      window = window.parent;
    }

    if (window !== options.window) {
      return false;
    }
  }

  return true;
}

// When a new worker appears, in some cases (i.e. the debugger is running) we
// want it to pause during registration until a later time (i.e. the debugger
// finishes attaching to the worker). This is an optional WorkderDebuggerManager
// listener that can be installed in addition to the WorkerTargetActorList
// listener. It always listens to new workers and pauses any which are matched
// by the WorkerTargetActorList.
function PauseMatchingWorkers(options) {
  this._options = options;
  this.onRegister = this._onRegister.bind(this);
  this.onUnregister = () => {};

  wdm.addListener(this);
}

PauseMatchingWorkers.prototype = {
  destroy() {
    wdm.removeListener(this);
  },

  _onRegister(dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      // Prevent the debuggee from executing in this worker until the debugger
      // has finished attaching to it.
      dbg.setDebuggerReady(false);
    }
  },
};

function WorkerTargetActorList(conn, options) {
  this._conn = conn;
  this._options = options;
  this._actors = new Map();
  this._onListChanged = null;
  this._pauseMatchingWorkers = null;
  this._mustNotify = false;
  this.onRegister = this.onRegister.bind(this);
  this.onUnregister = this.onUnregister.bind(this);
}

WorkerTargetActorList.prototype = {
  getList() {
    // Create a set of debuggers.
    const dbgs = new Set();
    for (const dbg of wdm.getWorkerDebuggerEnumerator()) {
      if (matchWorkerDebugger(dbg, this._options)) {
        dbgs.add(dbg);
      }
    }

    // Delete each actor for which we don't have a debugger.
    for (const [dbg] of this._actors) {
      if (!dbgs.has(dbg)) {
        this._actors.delete(dbg);
      }
    }

    // Create an actor for each debugger for which we don't have one.
    for (const dbg of dbgs) {
      if (!this._actors.has(dbg)) {
        this._actors.set(dbg, new WorkerTargetActor(this._conn, dbg));
      }
    }

    const actors = [];
    for (const [, actor] of this._actors) {
      actors.push(actor);
    }

    if (!this._mustNotify) {
      if (this._onListChanged !== null) {
        wdm.addListener(this);
      }
      this._mustNotify = true;
    }

    return Promise.resolve(actors);
  },

  get onListChanged() {
    return this._onListChanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }
    if (onListChanged === this._onListChanged) {
      return;
    }

    if (this._mustNotify) {
      if (this._onListChanged === null && onListChanged !== null) {
        wdm.addListener(this);
      }
      if (this._onListChanged !== null && onListChanged === null) {
        wdm.removeListener(this);
      }
    }
    this._onListChanged = onListChanged;
  },

  _notifyListChanged() {
    this._onListChanged();

    if (this._onListChanged !== null) {
      wdm.removeListener(this);
    }
    this._mustNotify = false;
  },

  onRegister(dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  },

  onUnregister(dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  },

  setPauseMatchingWorkers(shouldPause) {
    if (shouldPause != !!this._pauseMatchingWorkers) {
      if (shouldPause) {
        this._pauseMatchingWorkers = new PauseMatchingWorkers(this._options);
      } else {
        this._pauseMatchingWorkers.destroy();
        this._pauseMatchingWorkers = null;
      }
    }
  },
};

exports.WorkerTargetActorList = WorkerTargetActorList;
