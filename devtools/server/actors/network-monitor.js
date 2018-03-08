/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { networkMonitorSpec } = require("devtools/shared/specs/network-monitor");

loader.lazyRequireGetter(this, "NetworkMonitor", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "NetworkEventActor", "devtools/server/actors/network-event", true);

const NetworkMonitorActor = ActorClassWithSpec(networkMonitorSpec, {
  // Imported from WebConsole actor
  _netEvents: new Map(),
  _networkEventActorsByURL: new Map(),

  // NetworkMonitorActor is instanciated from WebConsoleActor.startListeners
  // Either in the same process, for debugging service worker requests or when debugger
  // the parent process itself and tracking chrome requests.
  // Or in another process, for tracking content requests that are actually done in the
  // parent process.
  // `filters` argument either contains an `outerWindowID` attribute when this is used
  // cross processes. Or a `window` attribute when instanciated in the same process.
  // `parentID` is an optional argument, to be removed, that specify the ID of the Web
  // console actor. This is used to fake emitting an event from it to prevent changing
  // RDP behavior.
  initialize(conn, filters, parentID) {
    Actor.prototype.initialize.call(this, conn);

    this.parentID = parentID;

    // Immediately start watching for new request according to `filters`.
    // NetworkMonitor will call `onNetworkEvent` method.
    this.netMonitor = new NetworkMonitor(filters, this);
    this.netMonitor.init();
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    if (this.netMonitor) {
      this.netMonitor.destroy();
      this.netMonitor = null;
    }
  },

  getNetworkEventActor(channelId) {
    let actor = this._netEvents.get(channelId);
    if (actor) {
      return actor;
    }

    actor = new NetworkEventActor(this);
    this.manage(actor);

    // map channel to actor so we can associate future events with it
    this._netEvents.set(channelId, actor);
    return actor;
  },

  // This method is called by NetworkMonitor instance when a new request is fired
  onNetworkEvent(event) {
    const { channelId } = event;

    const actor = this.getNetworkEventActor(channelId);
    this._netEvents.set(channelId, actor);

    actor.init(event);

    this._networkEventActorsByURL.set(actor._request.url, actor);

    const packet = {
      from: this.parentID,
      type: "networkEvent",
      eventActor: actor.form()
    };

    this.conn.send(packet);
    return actor;
  },

});
exports.NetworkMonitorActor = NetworkMonitorActor;
