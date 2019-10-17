/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { symbolIteratorSpec } = require("devtools/shared/specs/symbol-iterator");

/**
 * A SymbolIteratorFront provides a way to access to symbols
 * of an object efficiently, slice by slice.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A SymbolIteratorActor grip returned by the protocol via
 *        BrowsingContextTargetActor.enumSymbols request.
 */
class SymbolIteratorFront extends FrontClassWithSpec(symbolIteratorSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;
  }

  get actor() {
    return this._grip.actor;
  }

  /**
   * Get the total number of symbols available in the iterator.
   */
  get count() {
    return this._grip.count;
  }

  /**
   * Get a set of following symbols.
   *
   * @param start Number
   *        The index of the first symbol to fetch.
   * @param count Number
   *        The number of symbols to fetch.
   * @param callback Function
   *        The function called when we receive the symbols.
   */
  slice(start, count) {
    const argumentObject = { start, count };
    return super.slice(argumentObject);
  }

  form(form) {
    this._grip = form;
  }
}

exports.SymbolIteratorFront = SymbolIteratorFront;
registerFront(SymbolIteratorFront);
