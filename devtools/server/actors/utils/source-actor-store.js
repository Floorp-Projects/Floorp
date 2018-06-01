/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "getSourceURL", "devtools/server/actors/source", true);

/**
 * Keeps track of persistent sources across reloads and ties different
 * source instances to the same actor id so that things like
 * breakpoints survive reloads. ThreadSources uses this to force the
 * same actorID on a SourceActor.
 */
function SourceActorStore() {
  // source identifier --> actor id
  this._sourceActorIds = Object.create(null);
}

SourceActorStore.prototype = {
  /**
   * Lookup an existing actor id that represents this source, if available.
   */
  getReusableActorId: function(source, originalUrl) {
    const url = this.getUniqueKey(source, originalUrl);
    if (url && url in this._sourceActorIds) {
      return this._sourceActorIds[url];
    }
    return null;
  },

  /**
   * Update a source with an actorID.
   */
  setReusableActorId: function(source, originalUrl, actorID) {
    const url = this.getUniqueKey(source, originalUrl);
    if (url) {
      this._sourceActorIds[url] = actorID;
    }
  },

  /**
   * Make a unique URL from a source that identifies it across reloads.
   */
  getUniqueKey: function(source, originalUrl) {
    if (originalUrl) {
      // Original source from a sourcemap.
      return originalUrl;
    }

    return getSourceURL(source);
  }
};

exports.SourceActorStore = SourceActorStore;
