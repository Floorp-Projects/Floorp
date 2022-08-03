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
   * @param {DevToolsServerConnection} conn
   *    The server connection.
   * @param {TargetActor} targetActor
   *    The top-level Actor for this tab.
   */
  initialize(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.targetActor = targetActor;

    this.onTrackChange = this.pushChange.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    TrackChangeEmitter.on("track-change", this.onTrackChange);
    this.targetActor.on("will-navigate", this.onWillNavigate);

    this.changes = [];
  },

  destroy() {
    this.clearChanges();
    this.targetActor.off("will-navigate", this.onWillNavigate);
    TrackChangeEmitter.off("track-change", this.onTrackChange);
    protocol.Actor.prototype.destroy.call(this);
  },

  start() {
    /**
     * This function currently does nothing and returns nothing. It exists only
     * so that the client can trigger the creation of the ChangesActor through
     * the front, without triggering side effects, and with a sensible semantic
     * meaning.
     */
  },

  changeCount() {
    return this.changes.length;
  },

  change(index) {
    if (index >= 0 && index < this.changes.length) {
      // Return a copy of the change at index.
      return Object.assign({}, this.changes[index]);
    }
    // No change at that index -- return undefined.
    return undefined;
  },

  allChanges() {
    /**
     * This function is called by all change event consumers on the client
     * to get their initial state synchronized with the ChangesActor. We
     * set a flag when this function is called so we know that it's worthwhile
     * to send events.
     */
    this._changesHaveBeenRequested = true;
    return this.changes.slice();
  },

  /**
   * Handler for "will-navigate" event from the browsing context. The event is fired for
   * the host page and any nested resources, like iframes. The list of changes should be
   * cleared only when the host page navigates, ignoring any of its iframes.
   *
   * TODO: Clear changes made within sources in iframes when they navigate. Bug 1513940
   *
   * @param {Object} eventData
   *        Event data with these properties:
   *        {
   *          window: Object      // Window DOM object of the event source page
   *          isTopLevel: Boolean // true if the host page will navigate
   *          newURI: String      // URI towards which the page will navigate
   *          request: Object     // Request data.
   *        }
   */
  onWillNavigate(eventData) {
    if (eventData.isTopLevel) {
      this.clearChanges();
    }
  },

  pushChange(change) {
    this.changes.push(change);
    if (this._changesHaveBeenRequested) {
      this.emit("add-change", change);
    }
  },

  popChange() {
    const change = this.changes.pop();
    if (this._changesHaveBeenRequested) {
      this.emit("remove-change", change);
    }
    return change;
  },

  clearChanges() {
    this.changes.length = 0;
    if (this._changesHaveBeenRequested) {
      this.emit("clear-changes");
    }
  },
});

exports.ChangesActor = ChangesActor;
