/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the eventLoopLag actor.
 */

"use strict";

function run_test()
{
  let {EventLoopLagFront} = require("devtools/shared/fronts/eventlooplag");

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  // As seen in EventTracer.cpp
  let threshold = 20;
  let interval = 10;


  let front;
  let client = new DebuggerClient(DebuggerServer.connectPipe());

  // Start tracking event loop lags.
  client.connect().then(function () {
    client.listTabs(function (resp) {
      front = new EventLoopLagFront(client, resp);
      front.start().then(success => {
        do_check_true(success);
        front.once("event-loop-lag", gotLagEvent);
        do_execute_soon(lag);
      });
    });
  });

  // Force a lag
  function lag() {
    let start = new Date();
    let duration = threshold + interval + 1;
    while (true) {
      if (((new Date()) - start) > duration) {
        break;
      }
    }
  }

  // Got a lag event. The test will time out if the actor
  // fails to detect the lag.
  function gotLagEvent(time) {
    do_print("lag: " + time);
    do_check_true(time >= threshold);
    front.stop().then(() => {
      finishClient(client);
    });
  }

  do_test_pending();
}
