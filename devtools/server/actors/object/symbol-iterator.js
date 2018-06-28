/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { symbolIteratorSpec } = require("devtools/shared/specs/symbol-iterator");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

/**
 * Creates an actor to iterate over an object's symbols.
 *
 * @param objectActor ObjectActor
 *        The object actor.
 */
const SymbolIteratorActor  = protocol.ActorClassWithSpec(symbolIteratorSpec, {
  initialize(objectActor, conn) {
    protocol.Actor.prototype.initialize.call(this, conn);

    let symbols = [];
    if (DevToolsUtils.isSafeDebuggerObject(objectActor.obj)) {
      try {
        symbols = objectActor.obj.getOwnPropertySymbols();
      } catch (err) {
        // The above can throw when the debuggee does not subsume the object's
        // compartment, or for some WrappedNatives like Cu.Sandbox.
      }
    }

    this.iterator = {
      size: symbols.length,
      symbolDescription(index) {
        const symbol = symbols[index];
        return {
          name: symbol.toString(),
          descriptor: objectActor._propertyDescriptor(symbol)
        };
      }
    };
  },

  form() {
    return {
      type: this.typeName,
      actor: this.actorID,
      count: this.iterator.size
    };
  },

  slice({ start, count }) {
    const ownSymbols = [];
    for (let i = start, m = start + count; i < m; i++) {
      ownSymbols.push(this.iterator.symbolDescription(i));
    }
    return {
      ownSymbols
    };
  },

  all() {
    return this.slice({ start: 0, count: this.iterator.size });
  }
});

exports.SymbolIteratorActor = SymbolIteratorActor;
