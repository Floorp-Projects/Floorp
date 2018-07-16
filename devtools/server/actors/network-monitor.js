/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { networkMonitorSpec } = require("devtools/shared/specs/network-monitor");

loader.lazyRequireGetter(this, "NetworkMonitor", "devtools/shared/webconsole/network-monitor", true);
loader.lazyRequireGetter(this, "NetworkEventActor", "devtools/server/actors/network-event", true);

const NetworkMonitorActor = ActorClassWithSpec(networkMonitorSpec, {
  _netEvents: new Map(),
  _networkEventActorsByURL: new Map(),

  /**
   * NetworkMonitorActor is instanciated from WebConsoleActor.startListeners
   * Either in the same process, for debugging service worker requests or when debugging
   * the parent process itself and tracking chrome requests.
   * Or in another process, for tracking content requests that are actually done in the
   * parent process.
   *
   * @param object filters
   *        Contains an `outerWindowID` attribute when this is used across processes.
   *        Or a `window` attribute when instanciated in the same process.
   * @param number parentID (optional)
   *        To be removed, specify the ID of the Web console actor.
   *        This is used to fake emitting an event from it to prevent changing RDP
   *        behavior.
   * @param nsIMessageManager messageManager (optional)
   *        Passed only when it is instanciated across processes. This is the manager to
   *        use to communicate with the other process.
   * @param object stackTraceCollector (optional)
   *        When the actor runs in the same process than the requests we are inspecting,
   *        the web console actor hands over a shared instance to the stack trace
   *        collector.
   */
  initialize(conn, filters, parentID, messageManager, stackTraceCollector) {
    Actor.prototype.initialize.call(this, conn);

    this.parentID = parentID;
    this.messageManager = messageManager;
    this.stackTraceCollector = stackTraceCollector;

    // Immediately start watching for new request according to `filters`.
    // NetworkMonitor will call `onNetworkEvent` method.
    this.netMonitor = new NetworkMonitor(filters, this);
    this.netMonitor.init();

    if (this.messageManager) {
      this.stackTraces = new Map();
      this.onStackTraceAvailable = this.onStackTraceAvailable.bind(this);
      this.messageManager.addMessageListener("debug:request-stack-available",
        this.onStackTraceAvailable);
      this.destroy = this.destroy.bind(this);
      this.messageManager.addMessageListener("debug:destroy-network-monitor",
        this.destroy);
    }
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    if (this.netMonitor) {
      this.netMonitor.destroy();
      this.netMonitor = null;
    }

    if (this.messageManager) {
      this.stackTraces.clear();
      this.messageManager.removeMessageListener("debug:request-stack-available",
        this.onStackTraceAvailable);
      this.messageManager.removeMessageListener("debug:destroy-network-monitor",
        this.destroy);
      this.messageManager = null;
    }
  },

  onStackTraceAvailable(msg) {
    if (!msg.data.stacktrace) {
      this.stackTraces.delete(msg.data.channelId);
    } else {
      this.stackTraces.set(msg.data.channelId, msg.data.stacktrace);
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

    if (this.messageManager) {
      event.cause.stacktrace = this.stackTraces.get(channelId);
    } else {
      event.cause.stacktrace = this.stackTraceCollector.getStackTrace(channelId);
    }
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
