/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class WatchpointMap {
  constructor(thread) {
    this.thread = thread;
    this._watchpoints = new Map();
  }

  setWatchpoint(objActor, data) {
    const { property, label, watchpointType } = data;
    const obj = objActor.rawValue();

    if (this.has(obj, property)) {
      return;
    }

    const desc =
      Object.getOwnPropertyDescriptor(obj, property) ||
      objActor.obj.getOwnPropertyDescriptor(property);

    if (desc.set || desc.get || !desc.configurable) {
      return;
    }

    const pauseAndRespond = type => {
      const frame = this.thread.dbg.getNewestFrame();
      this.thread._pauseAndRespond(frame, {
        type: type,
        message: label,
      });
    };

    if (watchpointType === "get") {
      objActor.obj.defineProperty(property, {
        configurable: desc.configurable,
        enumerable: desc.enumerable,
        set: objActor.obj.makeDebuggeeValue(v => {
          desc.value = v;
        }),
        get: objActor.obj.makeDebuggeeValue(() => {
          const frame = this.thread.dbg.getNewestFrame();

          if (!this.thread.hasMoved(frame, "getWatchpoint")) {
            return false;
          }

          if (!this.thread.skipBreakpoints) {
            pauseAndRespond("getWatchpoint");
          }

          return desc.value;
        }),
      });
    }

    if (watchpointType === "set") {
      objActor.obj.defineProperty(property, {
        configurable: desc.configurable,
        enumerable: desc.enumerable,
        set: objActor.obj.makeDebuggeeValue(v => {
          const frame = this.thread.dbg.getNewestFrame();

          if (!this.thread.hasMoved(frame, "setWatchpoint")) {
            desc.value = v;
            return;
          }

          if (!this.thread.skipBreakpoints) {
            pauseAndRespond("setWatchpoint");
          }

          desc.value = v;
        }),
        get: objActor.obj.makeDebuggeeValue(() => {
          return desc.value;
        }),
      });
    }
  }

  add(objActor, data) {
    // Get the object's description before calling setWatchpoint,
    // otherwise we'll get the modified property descriptor instead
    const desc = objActor.obj.getOwnPropertyDescriptor(data.property);
    this.setWatchpoint(objActor, data);

    const objWatchpoints =
      this._watchpoints.get(objActor.rawValue()) || new Map();

    objWatchpoints.set(data.property, { ...data, desc });
    this._watchpoints.set(objActor.rawValue(), objWatchpoints);
  }

  has(obj, property) {
    const objWatchpoints = this._watchpoints.get(obj);
    return objWatchpoints && objWatchpoints.has(property);
  }

  get(obj, property) {
    const objWatchpoints = this._watchpoints.get(obj);
    return objWatchpoints && objWatchpoints.get(property);
  }

  remove(objActor, property) {
    const obj = objActor.rawValue();

    // This should remove watchpoints on all of the object's properties if
    // a property isn't passed in as an argument
    if (!property) {
      for (const objProperty in obj) {
        this.remove(objActor, objProperty);
      }
    }

    if (!this.has(obj, property)) {
      return;
    }

    const objWatchpoints = this._watchpoints.get(obj);
    const { desc } = objWatchpoints.get(property);

    objWatchpoints.delete(property);
    this._watchpoints.set(obj, objWatchpoints);

    // We should stop keeping track of an object if it no longer
    // has a watchpoint
    if (objWatchpoints.size == 0) {
      this._watchpoints.delete(obj);
    }

    objActor.obj.defineProperty(property, desc);
  }

  removeAll(objActor) {
    const objWatchpoints = this._watchpoints.get(objActor.rawValue());
    if (!objWatchpoints) {
      return;
    }

    for (const objProperty in objWatchpoints) {
      this.remove(objActor, objProperty);
    }
  }
}

exports.WatchpointMap = WatchpointMap;
