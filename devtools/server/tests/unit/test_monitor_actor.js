/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the monitor actor.
 */

"use strict";

function run_test() {
  let EventEmitter = require("devtools/shared/old-event-emitter");

  function MonitorClient(client, form) {
    this.client = client;
    this.actor = form.monitorActor;
    this.events = ["update"];

    EventEmitter.decorate(this);
    client.registerClient(this);
  }
  MonitorClient.prototype.destroy = function () {
    this.client.unregisterClient(this);
  };
  MonitorClient.prototype.start = function (callback) {
    this.client.request({
      to: this.actor,
      type: "start"
    }, callback);
  };
  MonitorClient.prototype.stop = function (callback) {
    this.client.request({
      to: this.actor,
      type: "stop"
    }, callback);
  };

  let monitor, client;

  // Start the monitor actor.
  get_chrome_actors((c, form) => {
    client = c;
    monitor = new MonitorClient(client, form);
    monitor.on("update", gotUpdate);
    monitor.start(update);
  });

  let time = Date.now();

  function update() {
    let event = {
      graph: "Test",
      curve: "test",
      value: 42,
      time: time,
    };
    Services.obs.notifyObservers(null, "devtools-monitor-update", JSON.stringify(event));
  }

  function gotUpdate(type, packet) {
    packet.data.forEach(function (event) {
      // Ignore updates that were not sent by this test.
      if (event.graph === "Test") {
        Assert.equal(event.curve, "test");
        Assert.equal(event.value, 42);
        Assert.equal(event.time, time);
        monitor.stop(function (response) {
          monitor.destroy();
          finishClient(client);
        });
      }
    });
  }

  do_test_pending();
}
