/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { changesSpec } = require("devtools/shared/specs/changes");
const TrackChangeEmitter = require("devtools/server/actors/utils/track-change-emitter");

/**
 * The ChangesActor stores a stack of changes made by devtools on
 * the document in the associated tab.
 */
const ChangesActor = protocol.ActorClassWithSpec(changesSpec, {
  /**
   * Create a ChangesActor.
   *
   * @param {DebuggerServerConnection} conn
   *    The server connection.
   * @param {TargetActor} targetActor
   *    The top-level Actor for this tab.
   */
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;

    this.onTrackChange = this.pushChange.bind(this);
    this.onWillNavigate = this.clearChanges.bind(this);

    TrackChangeEmitter.on("track-change", this.onTrackChange);
    this.targetActor.on("will-navigate", this.onWillNavigate);

    this.changes = [];
  },

  destroy: function() {
    this.clearChanges();
    this.targetActor.off("will-navigate", this.onWillNavigate);
    TrackChangeEmitter.off("track-change", this.onTrackChange);
    protocol.Actor.prototype.destroy.call(this);
  },

  start: function() {
    /**
     * This function currently does nothing and returns nothing. It exists only
     * so that the client can trigger the creation of the ChangesActor through
     * the front, without triggering side effects, and with a sensible semantic
     * meaning.
     */
  },

  changeCount: function() {
    return this.changes.length;
  },

  change: function(index) {
    if (index >= 0 && index < this.changes.length) {
      // Return a copy of the change at index.
      return Object.assign({}, this.changes[index]);
    }
    // No change at that index -- return undefined.
    return undefined;
  },

  allChanges: function() {
    /**
     * This function is called by all change event consumers on the client
     * to get their initial state synchronized with the ChangesActor. We
     * set a flag when this function is called so we know that it's worthwhile
     * to send events.
     */
    this._changesHaveBeenRequested = true;
    return this.changes.slice();
  },

  pushChange: function(change) {
    this.changes.push(change);
    if (this._changesHaveBeenRequested) {
      this.emit("add-change", change);
    }
  },

  popChange: function() {
    const change = this.changes.pop();
    if (this._changesHaveBeenRequested) {
      this.emit("remove-change", change);
    }
    return change;
  },

  clearChanges: function() {
    this.changes.length = 0;
    if (this._changesHaveBeenRequested) {
      this.emit("clear-changes");
    }
  },
});

exports.ChangesActor = ChangesActor;
