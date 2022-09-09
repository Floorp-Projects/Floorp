/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cm, Cr, components } = require("chrome");
const { ComponentUtils } = require("resource://gre/modules/ComponentUtils.jsm");

/**
 * This is a nsIChannelEventSink implementation that monitors channel redirects and
 * informs the registered StackTraceCollector about the old and new channels.
 */
const SINK_CLASS_DESCRIPTION = "NetworkMonitor Channel Event Sink";
const SINK_CLASS_ID = components.ID("{e89fa076-c845-48a8-8c45-2604729eba1d}");
const SINK_CONTRACT_ID = "@mozilla.org/network/monitor/channeleventsink;1";
const SINK_CATEGORY_NAME = "net-channel-event-sinks";

function ChannelEventSink() {
  this.wrappedJSObject = this;
  this.collectors = new Set();
}

ChannelEventSink.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIChannelEventSink"]),

  registerCollector(collector) {
    this.collectors.add(collector);
  },

  unregisterCollector(collector) {
    this.collectors.delete(collector);

    if (this.collectors.size == 0) {
      ChannelEventSinkFactory.unregister();
    }
  },

  // eslint-disable-next-line no-shadow
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    for (const collector of this.collectors) {
      try {
        collector.onChannelRedirect(oldChannel, newChannel, flags);
      } catch (ex) {
        console.error(
          "StackTraceCollector.onChannelRedirect threw an exception",
          ex
        );
      }
    }
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },
};

const ChannelEventSinkFactory = ComponentUtils.generateSingletonFactory(
  ChannelEventSink
);

ChannelEventSinkFactory.register = function() {
  const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  if (registrar.isCIDRegistered(SINK_CLASS_ID)) {
    return;
  }

  registrar.registerFactory(
    SINK_CLASS_ID,
    SINK_CLASS_DESCRIPTION,
    SINK_CONTRACT_ID,
    ChannelEventSinkFactory
  );

  Services.catMan.addCategoryEntry(
    SINK_CATEGORY_NAME,
    SINK_CONTRACT_ID,
    SINK_CONTRACT_ID,
    false,
    true
  );
};

ChannelEventSinkFactory.unregister = function() {
  const registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.unregisterFactory(SINK_CLASS_ID, ChannelEventSinkFactory);

  Services.catMan.deleteCategoryEntry(
    SINK_CATEGORY_NAME,
    SINK_CONTRACT_ID,
    false
  );
};

ChannelEventSinkFactory.getService = function() {
  // Make sure the ChannelEventSink service is registered before accessing it
  ChannelEventSinkFactory.register();

  return Cc[SINK_CONTRACT_ID].getService(Ci.nsIChannelEventSink)
    .wrappedJSObject;
};
exports.ChannelEventSinkFactory = ChannelEventSinkFactory;
