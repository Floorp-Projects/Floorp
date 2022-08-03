/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A SourceLocation represents a location in a source.
 *
 * @param SourceActor actor
 *        A SourceActor representing a source.
 * @param Number line
 *        A line within the given source.
 * @param Number column
 *        A column within the given line.
 */
function SourceLocation(actor, line, column) {
  this._connection = actor ? actor.conn : null;
  this._actorID = actor ? actor.actorID : undefined;
  this._line = line;
  this._column = column;
}

SourceLocation.prototype = {
  get sourceActor() {
    return this._connection ? this._connection.getActor(this._actorID) : null;
  },

  get url() {
    return this.sourceActor.url;
  },

  get line() {
    return this._line;
  },

  get column() {
    return this._column;
  },

  get sourceUrl() {
    return this.sourceActor.url;
  },

  equals(other) {
    return (
      this.sourceActor.url == other.sourceActor.url &&
      this.line === other.line &&
      (this.column === undefined ||
        other.column === undefined ||
        this.column === other.column)
    );
  },

  toJSON() {
    return {
      source: this.sourceActor.form(),
      line: this.line,
      column: this.column,
    };
  },
};

exports.SourceLocation = SourceLocation;

/**
 * A method decorator that ensures the actor is in the expected state before
 * proceeding. If the actor is not in the expected state, the decorated method
 * returns a rejected promise.
 *
 * The actor's state must be at this.state property.
 *
 * @param String expectedState
 *        The expected state.
 * @param String activity
 *        Additional info about what's going on.
 * @param Function methodFunc
 *        The actor method to proceed with when the actor is in the expected
 *        state.
 *
 * @returns Function
 *          The decorated method.
 */
function expectState(expectedState, methodFunc, activity) {
  return function(...args) {
    if (this.state !== expectedState) {
      const msg =
        `Wrong state while ${activity}:` +
        `Expected '${expectedState}', ` +
        `but current state is '${this.state}'.`;
      return Promise.reject(new Error(msg));
    }

    return methodFunc.apply(this, args);
  };
}

exports.expectState = expectState;

/**
 * Autobind method from a `bridge` property set on some actors where the
 * implementation is delegated to a separate class, and where `bridge` points
 * to an instance of this class.
 */
function actorBridgeWithSpec(methodName) {
  return function() {
    return this.bridge[methodName].apply(this.bridge, arguments);
  };
}
exports.actorBridgeWithSpec = actorBridgeWithSpec;
