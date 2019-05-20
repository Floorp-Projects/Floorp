/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor, ActorClassWithSpec } = require("devtools/shared/protocol");
const { networkMonitorSpec } = require("devtools/shared/specs/network-monitor");

loader.lazyRequireGetter(this, "NetworkObserver", "devtools/server/actors/network-monitor/network-observer", true);
loader.lazyRequireGetter(this, "NetworkEventActor", "devtools/server/actors/network-event", true);

const NetworkMonitorActor = ActorClassWithSpec(networkMonitorSpec, {
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
   * @param nsIMessageManager messageManager
   *        This is the manager to use to communicate with the console actor. When both
   *        netmonitor and console actor runs in the same process, this is an instance
   *        of MockMessageManager instead of a real message manager.
   */
  initialize(conn, filters, parentID, messageManager) {
    Actor.prototype.initialize.call(this, conn);

    // Map of all NetworkEventActor indexed by channel ID
    this._netEvents = new Map();

    // Map of all NetworkEventActor indexed by URL
    this._networkEventActorsByURL = new Map();

    this.parentID = parentID;
    this.messageManager = messageManager;

    // Immediately start watching for new request according to `filters`.
    // NetworkMonitor will call `onNetworkEvent` method.
    this.observer = new NetworkObserver(filters, this);
    this.observer.init();

    this.stackTraces = new Set();

    this.onStackTraceAvailable = this.onStackTraceAvailable.bind(this);
    this.onRequestContent = this.onRequestContent.bind(this);
    this.onSetPreference = this.onSetPreference.bind(this);
    this.onBlockRequest = this.onBlockRequest.bind(this);
    this.onUnblockRequest = this.onUnblockRequest.bind(this);
    this.onGetNetworkEventActor = this.onGetNetworkEventActor.bind(this);
    this.onDestroyMessage = this.onDestroyMessage.bind(this);

    this.startListening();
  },

  onDestroyMessage({ data }) {
    if (data.actorID == this.parentID) {
      this.destroy();
    }
  },

  startListening() {
    this.messageManager.addMessageListener("debug:request-stack-available",
      this.onStackTraceAvailable);
    this.messageManager.addMessageListener("debug:request-content:request",
      this.onRequestContent);
    this.messageManager.addMessageListener("debug:netmonitor-preference",
      this.onSetPreference);
    this.messageManager.addMessageListener("debug:block-request",
      this.onBlockRequest);
    this.messageManager.addMessageListener("debug:unblock-request",
      this.onUnblockRequest);
    this.messageManager.addMessageListener("debug:get-network-event-actor:request",
      this.onGetNetworkEventActor);
    this.messageManager.addMessageListener("debug:destroy-network-monitor",
      this.onDestroyMessage);
  },

  stopListening() {
    this.messageManager.removeMessageListener("debug:request-stack-available",
      this.onStackTraceAvailable);
    this.messageManager.removeMessageListener("debug:request-content:request",
      this.onRequestContent);
    this.messageManager.removeMessageListener("debug:netmonitor-preference",
      this.onSetPreference);
    this.messageManager.removeMessageListener("debug:block-request",
      this.onBlockRequest);
    this.messageManager.removeMessageListener("debug:unblock-request",
      this.onUnblockRequest);
    this.messageManager.removeMessageListener("debug:get-network-event-actor:request",
      this.onGetNetworkEventActor);
    this.messageManager.removeMessageListener("debug:destroy-network-monitor",
      this.onDestroyMessage);
  },

  destroy() {
    Actor.prototype.destroy.call(this);

    if (this.observer) {
      this.observer.destroy();
      this.observer = null;
    }

    this.stackTraces.clear();
    if (this.messageManager) {
      this.stopListening();
      this.messageManager = null;
    }
  },

  /**
   * onBrowserSwap is called by the server when a browser frame swap occurs (typically
   * switching on/off RDM) and a new message manager should be used.
   */
  onBrowserSwap(mm) {
    this.stopListening();
    this.messageManager = mm;
    this.stackTraces = new Set();
    this.startListening();
  },

  onStackTraceAvailable(msg) {
    const { channelId } = msg.data;
    if (!msg.data.stacktrace) {
      this.stackTraces.delete(channelId);
    } else {
      this.stackTraces.add(channelId);
    }
  },

  getRequestContentForActor(actor) {
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
    const actor = this._networkEventActorsByURL.get(url);
    // Always reply with a message, but with a null `content` if this instance
    // did not processed this request
    const content = actor ? this.getRequestContentForActor(actor) : null;
    this.messageManager.sendAsyncMessage("debug:request-content:response", {
      url,
      content,
    });
  },

  onSetPreference({ data }) {
    if ("saveRequestAndResponseBodies" in data) {
      this.observer.saveRequestAndResponseBodies = data.saveRequestAndResponseBodies;
    }
    if ("throttleData" in data) {
      this.observer.throttleData = data.throttleData;
    }
  },

  onBlockRequest({ data }) {
    const { filter } = data;
    this.observer.blockRequest(filter);
  },

  onUnblockRequest({ data }) {
    const { filter } = data;
    this.observer.unblockRequest(filter);
  },

  onGetNetworkEventActor({ data }) {
    const actor = this.getNetworkEventActor(data.channelId);
    this.messageManager.sendAsyncMessage("debug:get-network-event-actor:response", {
      channelId: data.channelId,
      actor: actor.form(),
    });
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
    const { channelId, cause, url } = event;

    const actor = this.getNetworkEventActor(channelId);
    this._netEvents.set(channelId, actor);

    // Find the ID which the stack trace collector will use to save this
    // channel's stack trace.
    let id;
    if (cause.type == "websocket") {
      // Use the URL, but convert from the http URL which this channel uses to
      // the original websocket URL which triggered this channel's construction.
      id = url.replace(/^http/, "ws");
    } else {
      id = channelId;
    }

    event.cause.stacktrace = this.stackTraces.has(id);
    if (event.cause.stacktrace) {
      this.stackTraces.delete(id);
    }
    actor.init(event);

    this._networkEventActorsByURL.set(actor._request.url, actor);

    this.conn.sendActorEvent(this.parentID, "networkEvent", { eventActor: actor.form() });
    return actor;
  },

});
exports.NetworkMonitorActor = NetworkMonitorActor;
