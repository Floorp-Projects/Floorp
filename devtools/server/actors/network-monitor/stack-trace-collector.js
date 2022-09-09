/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, components } = require("chrome");

loader.lazyRequireGetter(
  this,
  "ChannelEventSinkFactory",
  "devtools/server/actors/network-monitor/channel-event-sink",
  true
);
loader.lazyRequireGetter(
  this,
  "NetworkUtils",
  "devtools/server/actors/network-monitor/utils/network-utils"
);
loader.lazyRequireGetter(
  this,
  "WebConsoleUtils",
  "devtools/server/actors/webconsole/utils",
  true
);

function StackTraceCollector(filters, netmonitors) {
  this.filters = filters;
  this.stacktracesById = new Map();
  this.netmonitors = netmonitors;
}

StackTraceCollector.prototype = {
  init() {
    Services.obs.addObserver(this, "http-on-opening-request");
    Services.obs.addObserver(this, "document-on-opening-request");
    Services.obs.addObserver(this, "network-monitor-alternate-stack");
    ChannelEventSinkFactory.getService().registerCollector(this);
    this.onGetStack = this.onGetStack.bind(this);
    for (const { messageManager } of this.netmonitors) {
      messageManager.addMessageListener(
        "debug:request-stack:request",
        this.onGetStack
      );
    }
  },

  destroy() {
    Services.obs.removeObserver(this, "http-on-opening-request");
    Services.obs.removeObserver(this, "document-on-opening-request");
    Services.obs.removeObserver(this, "network-monitor-alternate-stack");
    ChannelEventSinkFactory.getService().unregisterCollector(this);
    for (const { messageManager } of this.netmonitors) {
      messageManager.removeMessageListener(
        "debug:request-stack:request",
        this.onGetStack
      );
    }
  },

  _saveStackTrace(id, stacktrace) {
    if (this.stacktracesById.has(id)) {
      // We can get up to two stack traces for the same channel: one each from
      // the two observer topics we are listening to. Use the first stack trace
      // which is specified, and ignore any later one.
      return;
    }
    for (const { messageManager } of this.netmonitors) {
      messageManager.sendAsyncMessage("debug:request-stack-available", {
        channelId: id,
        stacktrace: stacktrace && !!stacktrace.length,
        lastFrame: stacktrace && stacktrace.length ? stacktrace[0] : undefined,
      });
    }
    this.stacktracesById.set(id, stacktrace);
  },

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
        // WebSocketChannels do not have IDs, so use the URL. When a WebSocket is
        // opened in a content process, a channel is created locally but the HTTP
        // channel for the connection lives entirely in the parent process. When
        // the server code running in the parent sees that HTTP channel, it will
        // look for the creation stack using the websocket's URL.
        try {
          channel = subject.QueryInterface(Ci.nsIWebSocketChannel);
        } catch (e3) {
          // Channels which don't implement the above interfaces can appear here,
          // such as nsIFileChannel. Ignore these channels.
          return;
        }
        id = channel.URI.spec;
      }
    }

    if (!NetworkUtils.matchRequest(channel, this.filters)) {
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

    this._saveStackTrace(id, stacktrace);
  },

  // eslint-disable-next-line no-shadow
  onChannelRedirect(oldChannel, newChannel, flags) {
    // We can be called with any nsIChannel, but are interested only in HTTP channels
    try {
      oldChannel.QueryInterface(Ci.nsIHttpChannel);
      newChannel.QueryInterface(Ci.nsIHttpChannel);
    } catch (ex) {
      return;
    }

    const oldId = oldChannel.channelId;
    const stacktrace = this.stacktracesById.get(oldId);
    if (stacktrace) {
      this._saveStackTrace(newChannel.channelId, stacktrace);
    }
  },

  getStackTrace(channelId) {
    const trace = this.stacktracesById.get(channelId);
    this.stacktracesById.delete(channelId);
    return WebConsoleUtils.removeFramesAboveDebuggerEval(trace);
  },

  onGetStack(msg) {
    const messageManager = msg.target;
    const channelId = msg.data;
    const stack = this.getStackTrace(channelId);
    messageManager.sendAsyncMessage("debug:request-stack:response", {
      channelId,
      stack,
    });
  },
};

exports.StackTraceCollector = StackTraceCollector;
