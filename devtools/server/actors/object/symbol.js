/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { symbolSpec } = require("devtools/shared/specs/symbol");
loader.lazyRequireGetter(this, "createValueGrip", "devtools/server/actors/object/utils", true);

/**
 * Creates an actor for the specified symbol.
 *
 * @param symbol Symbol
 *        The symbol.
 */
const SymbolActor = protocol.ActorClassWithSpec(symbolSpec, {
  initialize(symbol) {
    protocol.Actor.prototype.initialize.call(this);
    this.symbol = symbol;
  },

  rawValue: function() {
    return this.symbol;
  },

  destroy: function() {
    // Because symbolActors is not a weak map, we won't automatically leave
    // it so we need to manually leave on destroy so that we don't leak
    // memory.
    this._releaseActor();
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  form: function() {
    let form = {
      type: this.typeName,
      actor: this.actorID,
    };
    let name = getSymbolName(this.symbol);
    if (name !== undefined) {
      // Create a grip for the name because it might be a longString.
      form.name = createValueGrip(name, this.registeredPool);
    }
    return form;
  },

  /**
   * Handle a request to release this SymbolActor instance.
   */
  release: function() {
    // TODO: also check if registeredPool === threadActor.threadLifetimePool
    // when the web console moves away from manually releasing pause-scoped
    // actors.
    this._releaseActor();
    this.registeredPool.removeActor(this);
    return {};
  },

  _releaseActor: function() {
    if (this.registeredPool && this.registeredPool.symbolActors) {
      delete this.registeredPool.symbolActors[this.symbol];
    }
  }
});

const symbolProtoToString = Symbol.prototype.toString;

function getSymbolName(symbol) {
  const name = symbolProtoToString.call(symbol).slice("Symbol(".length, -1);
  return name || undefined;
}

/**
 * Create a grip for the given symbol.
 *
 * @param sym Symbol
 *        The symbol we are creating a grip for.
 * @param pool ActorPool
 *        The actor pool where the new actor will be added.
 */
function symbolGrip(sym, pool) {
  if (!pool.symbolActors) {
    pool.symbolActors = Object.create(null);
  }

  if (sym in pool.symbolActors) {
    return pool.symbolActors[sym].form();
  }

  let actor = new SymbolActor(sym);
  pool.addActor(actor);
  pool.symbolActors[sym] = actor;
  return actor.form();
}

module.exports = {
  SymbolActor,
  symbolGrip,
};
