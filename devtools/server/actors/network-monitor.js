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
      this.stackTraces = new Set();
      this.onStackTraceAvailable = this.onStackTraceAvailable.bind(this);
      this.messageManager.addMessageListener("debug:request-stack-available",
        this.onStackTraceAvailable);
      this.onRequestContent = this.onRequestContent.bind(this);
      this.messageManager.addMessageListener("debug:request-content",
        this.onRequestContent);
      this.onSetPreference = this.onSetPreference.bind(this);
      this.messageManager.addMessageListener("debug:netmonitor-preference",
        this.onSetPreference);
      this.onGetNetworkEventActor = this.onGetNetworkEventActor.bind(this);
      this.messageManager.addMessageListener("debug:get-network-event-actor",
        this.onGetNetworkEventActor);
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
      this.messageManager.removeMessageListener("debug:request-content",
        this.onRequestContent);
      this.messageManager.removeMessageListener("debug:netmonitor-preference",
        this.onSetPreference);
      this.messageManager.removeMessageListener("debug:get-network-event-actor",
        this.onGetNetworkEventActor);
      this.messageManager.removeMessageListener("debug:destroy-network-monitor",
        this.destroy);
      this.messageManager = null;
    }
  },

  onStackTraceAvailable(msg) {
    const { channelId } = msg.data;
    if (!msg.data.stacktrace) {
      this.stackTraces.delete(channelId);
    } else {
      this.stackTraces.add(channelId);
    }
  },

  getRequestContentForURL(url) {
    const actor = this._networkEventActorsByURL.get(url);
    if (!actor) {
      return null;
    }
    const content = actor._response.content;
    if (actor._discardResponseBody || actor._truncated || !content || !content.size) {
      // Do not return the stylesheet text if there is no meaningful content or if it's
      // still loading. Let the caller handle it by doing its own separate request.
      return null;
    }

    if (content.text.type != "longString") {
      // For short strings, the text is available directly.
      return {
        content: content.text,
        contentType: content.mimeType,
      };
    }
    // For long strings, look up the actor that holds the full text.
    const longStringActor = this.conn._getOrCreateActor(content.text.actor);
    if (!longStringActor) {
      return null;
    }
    return {
      content: longStringActor.str,
      contentType: content.mimeType,
    };
  },

  onRequestContent(msg) {
    const { url } = msg.data;
    const content = this.getRequestContentForURL(url);
    this.messageManager.sendAsyncMessage("debug:request-content", {
      url,
      content,
    });
  },

  onSetPreference({ data }) {
    if ("saveRequestAndResponseBodies" in data) {
      this.netMonitor.saveRequestAndResponseBodies = data.saveRequestAndResponseBodies;
    }
    if ("throttleData" in data) {
      this.netMonitor.throttleData = data.throttleData;
    }
  },

  onGetNetworkEventActor({ data }) {
    const actor = this.getNetworkEventActor(data.channelId);
    this.messageManager.sendAsyncMessage("debug:get-network-event-actor", actor.form());
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
      event.cause.stacktrace = this.stackTraces.has(channelId);
      if (event.cause.stacktrace) {
        this.stackTraces.delete(channelId);
      }
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
