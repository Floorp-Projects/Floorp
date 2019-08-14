/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BreakpointActor } = require("devtools/server/actors/breakpoint");

/**
 * A BreakpointActorMap is a map from locations to instances of BreakpointActor.
 */
function BreakpointActorMap(threadActor) {
  this._threadActor = threadActor;
  this._actors = {};
}

BreakpointActorMap.prototype = {
  // Get the key in the _actors table for a given breakpoint location.
  // See also duplicate code in commands.js :(
  _locationKey(location) {
    const { sourceUrl, sourceId, line, column } = location;
    return `${sourceUrl}:${sourceId}:${line}:${column}`;
  },

  /**
   * Return all BreakpointActors in this BreakpointActorMap.
   */
  findActors() {
    return Object.values(this._actors);
  },

  listKeys() {
    return Object.keys(this._actors);
  },

  /**
   * Return the BreakpointActor at the given location in this
   * BreakpointActorMap.
   *
   * @param BreakpointLocation location
   *        The location for which the BreakpointActor should be returned.
   *
   * @returns BreakpointActor actor
   *          The BreakpointActor at the given location.
   */
  getOrCreateBreakpointActor(location) {
    const key = this._locationKey(location);
    if (!this._actors[key]) {
      this._actors[key] = new BreakpointActor(this._threadActor, location);
    }
    return this._actors[key];
  },

  get(location) {
    const key = this._locationKey(location);
    return this._actors[key];
  },

  /**
   * Delete the BreakpointActor from the given location in this
   * BreakpointActorMap.
   *
   * @param BreakpointLocation location
   *        The location from which the BreakpointActor should be deleted.
   */
  deleteActor(location) {
    const key = this._locationKey(location);
    delete this._actors[key];
  },
};

exports.BreakpointActorMap = BreakpointActorMap;
