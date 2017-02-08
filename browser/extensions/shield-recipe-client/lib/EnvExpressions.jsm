/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/TelemetryArchive.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://shield-recipe-client/lib/Sampling.jsm");

const {generateUUID} = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

this.EXPORTED_SYMBOLS = ["EnvExpressions"];

const prefs = Services.prefs.getBranch("extensions.shield-recipe-client.");

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
    bucketSample: Sampling.bucketSample,
  });
  return jexl;
});

this.EnvExpressions = {
  getLatestTelemetry: Task.async(function *() {
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
  }),

  getUserId() {
    let id = prefs.getCharPref("user_id");
    if (id === "") {
      // generateUUID adds leading and trailing "{" and "}". strip them off.
      id = generateUUID().toString().slice(1, -1);
      prefs.setCharPref("user_id", id);
    }
    return id;
  },

  eval(expr, extraContext = {}) {
    // First clone the extra context
    const context = Object.assign({normandy: {}}, extraContext);
    // jexl handles promises, so it is fine to include them in this data.
    context.telemetry = EnvExpressions.getLatestTelemetry();

    context.normandy = Object.assign(context.normandy, {
      userId: EnvExpressions.getUserId(),
      distribution: Preferences.get("distribution.id", "default"),
    });

    const onelineExpr = expr.replace(/[\t\n\r]/g, " ");
    return jexl.eval(onelineExpr, context);
  },
};
