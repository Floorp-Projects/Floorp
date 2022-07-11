/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { NETWORK_EVENT_STACKTRACE },
} = require("devtools/server/actors/resources/index");

const { Ci, components } = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "ChannelEventSinkFactory",
  "devtools/server/actors/network-monitor/channel-event-sink",
  true
);

loader.lazyRequireGetter(
  this,
  "NetworkUtils",
  "devtools/server/actors/network-monitor/utils/network-utils.js"
);

class NetworkEventStackTracesWatcher {
  /**
   * Start watching for all network event's stack traces related to a given Target actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should observe the strack traces
   * @param Object options
   *        Dictionary object with following attributes:
   *        - onAvailable: mandatory
   *          This will be called for each resource.
   */
  async watch(targetActor, { onAvailable }) {
    this.stacktraces = new Map();
    this.onStackTraceAvailable = onAvailable;
    this.targetActor = targetActor;

    Services.obs.addObserver(this, "http-on-opening-request");
    Services.obs.addObserver(this, "document-on-opening-request");
    Services.obs.addObserver(this, "network-monitor-alternate-stack");
    ChannelEventSinkFactory.getService().registerCollector(this);
  }

  /**
   * Stop watching for network event's strack traces related to a given Target Actor.
   *
   * @param TargetActor targetActor
   *        The target actor from which we should stop observing the strack traces
   */
  destroy(targetActor) {
    this.stacktraces.clear();
    Services.obs.removeObserver(this, "http-on-opening-request");
    Services.obs.removeObserver(this, "document-on-opening-request");
    Services.obs.removeObserver(this, "network-monitor-alternate-stack");
    ChannelEventSinkFactory.getService().unregisterCollector(this);
  }

  onChannelRedirect(oldChannel, newChannel, flags) {
    // We can be called with any nsIChannel, but are interested only in HTTP channels
    try {
      oldChannel.QueryInterface(Ci.nsIHttpChannel);
      newChannel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }

    const oldId = oldChannel.channelId;
    const stacktrace = this.stacktraces.get(oldId);
    if (stacktrace) {
      this._setStackTrace(newChannel.channelId, stacktrace);
    }
  }

  observe(subject, topic, data) {
    let channel, id;
    try {
      // We need to QI nsIHttpChannel in order to load the interface's
      // methods / attributes for later code that could assume we are dealing
      // with a nsIHttpChannel.
      channel = subject.QueryInterface(Ci.nsIHttpChannel);
      id = channel.channelId;
    } catch (e1) {
      try {
        channel = subject.QueryInterface(Ci.nsIIdentChannel);
        id = channel.channelId;
      } catch (e2) {
        // WebSocketChannels do not have IDs, so use the serial. When a WebSocket is
        // opened in a content process, a channel is created locally but the HTTP
        // channel for the connection lives entirely in the parent process. When
        // the server code running in the parent sees that HTTP channel, it will
        // look for the creation stack using the websocket's serial.
        try {
          channel = subject.QueryInterface(Ci.nsIWebSocketChannel);
          id = channel.serial;
        } catch (e3) {
          // Channels which don't implement the above interfaces can appear here,
          // such as nsIFileChannel. Ignore these channels.
          return;
        }
      }
    }

    if (
      !NetworkUtils.matchRequest(channel, {
        targetActor: this.targetActor,
      })
    ) {
      return;
    }

    if (this.stacktraces.has(id)) {
      // We can get up to two stack traces for the same channel: one each from
      // the two observer topics we are listening to. Use the first stack trace
      // which is specified, and ignore any later one.
      return;
    }

    const stacktrace = [];
    switch (topic) {
      case "http-on-opening-request":
      case "document-on-opening-request": {
        // The channel is being opened on the main thread, associate the current
        // stack with it.
        //
        // Convert the nsIStackFrame XPCOM objects to a nice JSON that can be
        // passed around through message managers etc.
        let frame = components.stack;
        if (frame?.caller) {
          frame = frame.caller;
          while (frame) {
            stacktrace.push({
              filename: frame.filename,
              lineNumber: frame.lineNumber,
              columnNumber: frame.columnNumber,
              functionName: frame.name,
              asyncCause: frame.asyncCause,
            });
            frame = frame.caller || frame.asyncCaller;
          }
        }
        break;
      }
      case "network-monitor-alternate-stack": {
        // An alternate stack trace is being specified for this channel.
        // The topic data is the JSON for the saved frame stack we should use,
        // so convert this into the expected format.
        //
        // This topic is used in the following cases:
        //
        // - The HTTP channel is opened asynchronously or on a different thread
        //   from the code which triggered its creation, in which case the stack
        //   from components.stack will be empty. The alternate stack will be
        //   for the point we want to associate with the channel.
        //
        // - The channel is not a nsIHttpChannel, and we will receive no
        //   opening request notification for it.
        let frame = JSON.parse(data);
        while (frame) {
          stacktrace.push({
            filename: frame.source,
            lineNumber: frame.line,
            columnNumber: frame.column,
            functionName: frame.functionDisplayName,
            asyncCause: frame.asyncCause,
          });
          frame = frame.parent || frame.asyncParent;
        }
        break;
      }
      default:
        throw new Error("Unexpected observe() topic");
    }

    this._setStackTrace(id, stacktrace);
  }

  _setStackTrace(resourceId, stacktrace) {
    this.stacktraces.set(resourceId, stacktrace);
    this.onStackTraceAvailable([
      {
        resourceType: NETWORK_EVENT_STACKTRACE,
        resourceId,
        stacktraceAvailable: stacktrace && stacktrace.length > 0,
        lastFrame:
          stacktrace && stacktrace.length > 0 ? stacktrace[0] : undefined,
      },
    ]);
  }

  getStackTrace(id) {
    let stacktrace = [];
    if (this.stacktraces.has(id)) {
      stacktrace = this.stacktraces.get(id);
    }
    return stacktrace;
  }
}
module.exports = NetworkEventStackTracesWatcher;
