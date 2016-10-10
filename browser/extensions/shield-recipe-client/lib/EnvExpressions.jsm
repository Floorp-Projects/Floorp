/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/TelemetryArchive.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://shield-recipe-client/lib/Sampling.jsm");
Cu.import("resource://gre/modules/Log.jsm");

this.EXPORTED_SYMBOLS = ["EnvExpressions"];

XPCOMUtils.defineLazyGetter(this, "nodeRequire", () => {
  const {Loader, Require} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
  const loader = new Loader({
    paths: {
      "": "resource://shield-recipe-client/node_modules/",
    },
  });
  return new Require(loader, {});
});

XPCOMUtils.defineLazyGetter(this, "jexl", () => {
  const {Jexl} = nodeRequire("jexl/lib/Jexl.js");
  const jexl = new Jexl();
  jexl.addTransforms({
    date: dateString => new Date(dateString),
    stableSample: Sampling.stableSample,
  });
  return jexl;
});

const getLatestTelemetry = Task.async(function *() {
  const pings = yield TelemetryArchive.promiseArchivedPingList();

  // get most recent ping per type
  const mostRecentPings = {};
  for (const ping of pings) {
    if (ping.type in mostRecentPings) {
      if (mostRecentPings[ping.type].timeStampCreated < ping.timeStampCreated) {
        mostRecentPings[ping.type] = ping;
      }
    } else {
      mostRecentPings[ping.type] = ping;
    }
  }

  const telemetry = {};
  for (const key in mostRecentPings) {
    const ping = mostRecentPings[key];
    telemetry[ping.type] = yield TelemetryArchive.promiseArchivedPingById(ping.id);
  }
  return telemetry;
});

this.EnvExpressions = {
  eval(expr, extraContext = {}) {
    const context = Object.assign({telemetry: getLatestTelemetry()}, extraContext);
    const onelineExpr = expr.replace(/[\t\n\r]/g, " ");
    return jexl.eval(onelineExpr, context);
  },
};
