/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ObjectActor } = require("resource://devtools/server/actors/object.js");

class PauseScopedObjectActor extends ObjectActor {
  /**
   * Creates a pause-scoped actor for the specified object.
   * @see ObjectActor
   */
  constructor(obj, hooks, conn) {
    super(obj, hooks, conn);

    this.hooks.promote = hooks.promote;
    this.hooks.isThreadLifetimePool = hooks.isThreadLifetimePool;

    const guardWithPaused = [
      "decompile",
      "displayString",
      "ownPropertyNames",
      "parameterNames",
      "property",
      "prototype",
      "prototypeAndProperties",
      "scope",
    ];

    for (const methodName of guardWithPaused) {
      this[methodName] = this.withPaused(this[methodName]);
    }

    /**
     * Handle a protocol request to promote a pause-lifetime grip to a
     * thread-lifetime grip.
     */
    this.threadGrip = this.withPaused(function () {
      this.hooks.promote();
      return {};
    });
  }

  isPaused() {
    return this.threadActor ? this.threadActor.state === "paused" : true;
  }

  withPaused(method) {
    return function () {
      if (this.isPaused()) {
        return method.apply(this, arguments);
      }

      return {
        error: "wrongState",
        message:
          this.constructor.name +
          " actors can only be accessed while the thread is paused.",
      };
    };
  }

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   */
  destroy() {
    if (this.hooks.isThreadLifetimePool()) {
      return {
        error: "notReleasable",
        message: "Only thread-lifetime actors can be released.",
      };
    }

    super.destroy();
    return null;
  }
}

exports.PauseScopedObjectActor = PauseScopedObjectActor;
