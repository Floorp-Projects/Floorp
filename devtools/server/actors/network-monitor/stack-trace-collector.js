/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci, components} = require("chrome");
const Services = require("Services");

loader.lazyRequireGetter(this, "ChannelEventSinkFactory",
                         "devtools/server/actors/network-monitor/channel-event-sink",
                         true);
loader.lazyRequireGetter(this, "matchRequest",
                         "devtools/server/actors/network-monitor/network-observer",
                         true);

function StackTraceCollector(filters, netmonitors) {
  this.filters = filters;
  this.stacktracesById = new Map();
  this.netmonitors = netmonitors;
}

StackTraceCollector.prototype = {
  init() {
    Services.obs.addObserver(this, "http-on-opening-request");
    ChannelEventSinkFactory.getService().registerCollector(this);
    this.onGetStack = this.onGetStack.bind(this);
    for (const { messageManager } of this.netmonitors) {
      messageManager.addMessageListener("debug:request-stack:request", this.onGetStack);
    }
  },

  destroy() {
    Services.obs.removeObserver(this, "http-on-opening-request");
    ChannelEventSinkFactory.getService().unregisterCollector(this);
    for (const { messageManager } of this.netmonitors) {
      messageManager.removeMessageListener("debug:request-stack:request",
        this.onGetStack);
    }
  },

  _saveStackTrace(channel, stacktrace) {
    for (const { messageManager } of this.netmonitors) {
      messageManager.sendAsyncMessage("debug:request-stack-available", {
        channelId: channel.channelId,
        stacktrace: stacktrace && stacktrace.length > 0,
      });
    }
    this.stacktracesById.set(channel.channelId, stacktrace);
  },

  observe(subject) {
    const channel = subject.QueryInterface(Ci.nsIHttpChannel);

    if (!matchRequest(channel, this.filters)) {
      return;
    }

    // Convert the nsIStackFrame XPCOM objects to a nice JSON that can be
    // passed around through message managers etc.
    let frame = components.stack;
    const stacktrace = [];
    if (frame && frame.caller) {
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

    this._saveStackTrace(channel, stacktrace);
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
      this._saveStackTrace(newChannel, stacktrace);
    }
  },

  getStackTrace(channelId) {
    const trace = this.stacktracesById.get(channelId);
    this.stacktracesById.delete(channelId);
    return trace;
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
