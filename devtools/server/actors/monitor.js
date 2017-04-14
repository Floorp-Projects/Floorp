/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci, Cc} = require("chrome");
const {XPCOMUtils} = require("resource://gre/modules/XPCOMUtils.jsm");
const Services = require("Services");

function MonitorActor(connection) {
  this.conn = connection;
  this._updates = [];
  this._started = false;
}

MonitorActor.prototype = {
  actorPrefix: "monitor",

  // Updates.

  _sendUpdate: function () {
    if (this._started) {
      this.conn.sendActorEvent(this.actorID, "update", { data: this._updates });
      this._updates = [];
    }
  },

  // Methods available from the front.

  start: function () {
    if (!this._started) {
      this._started = true;
      Services.obs.addObserver(this, "devtools-monitor-update");
      Services.obs.notifyObservers(null, "devtools-monitor-start", "");
      this._agents.forEach(agent => this._startAgent(agent));
    }
    return {};
  },

  stop: function () {
    if (this._started) {
      this._agents.forEach(agent => agent.stop());
      Services.obs.notifyObservers(null, "devtools-monitor-stop", "");
      Services.obs.removeObserver(this, "devtools-monitor-update");
      this._started = false;
    }
    return {};
  },

  destroy: function () {
    this.stop();
  },

  // nsIObserver.

  observe: function (subject, topic, data) {
    if (topic == "devtools-monitor-update") {
      try {
        data = JSON.parse(data);
      } catch (e) {
        console.error("Observer notification data is not a valid JSON-string:",
                      data, e.message);
        return;
      }
      if (!Array.isArray(data)) {
        this._updates.push(data);
      } else {
        this._updates = this._updates.concat(data);
      }
      this._sendUpdate();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  // Update agents (see USSAgent for an example).

  _agents: [],

  _startAgent: function (agent) {
    try {
      agent.start();
    } catch (e) {
      this._removeAgent(agent);
    }
  },

  _addAgent: function (agent) {
    this._agents.push(agent);
    if (this._started) {
      this._startAgent(agent);
    }
  },

  _removeAgent: function (agent) {
    let index = this._agents.indexOf(agent);
    if (index > -1) {
      this._agents.splice(index, 1);
    }
  },
};

MonitorActor.prototype.requestTypes = {
  "start": MonitorActor.prototype.start,
  "stop": MonitorActor.prototype.stop,
};

exports.MonitorActor = MonitorActor;

var USSAgent = {
  _mgr: null,
  _timeout: null,
  _packet: {
    graph: "USS",
    time: null,
    value: null
  },

  start: function () {
    USSAgent._mgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
      Ci.nsIMemoryReporterManager);
    if (!USSAgent._mgr.residentUnique) {
      throw new Error("Couldn't get USS.");
    }
    USSAgent.update();
  },

  update: function () {
    if (!USSAgent._mgr) {
      USSAgent.stop();
      return;
    }
    USSAgent._packet.time = Date.now();
    USSAgent._packet.value = USSAgent._mgr.residentUnique;
    Services.obs.notifyObservers(null, "devtools-monitor-update",
      JSON.stringify(USSAgent._packet));
    USSAgent._timeout = setTimeout(USSAgent.update, 300);
  },

  stop: function () {
    clearTimeout(USSAgent._timeout);
    USSAgent._mgr = null;
  }
};

MonitorActor.prototype._addAgent(USSAgent);
