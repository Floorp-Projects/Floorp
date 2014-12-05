"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  requestLongerTimeout(2);
  UITourTest();
}

let tests = [
  function test_no_params(done) {
    function listener(event, params) {
      is(event, "test-event-1", "Correct event name");
      is(params, null, "No param object");
      gContentAPI.observe(null);
      done();
    }

    gContentAPI.observe(listener, () => {
      UITour.notify("test-event-1");
    });
  },
  function test_param_string(done) {
    function listener(event, params) {
      is(event, "test-event-2", "Correct event name");
      is(params, "a param", "Correct param string");
      gContentAPI.observe(null);
      done();
    }

    gContentAPI.observe(listener, () => {
      UITour.notify("test-event-2", "a param");
    });
  },
  function test_param_object(done) {
    function listener(event, params) {
      is(event, "test-event-3", "Correct event name");
      is(JSON.stringify(params), JSON.stringify({key: "something"}), "Correct param object");
      gContentAPI.observe(null);
      done();
    }

    gContentAPI.observe(listener, () => {
      UITour.notify("test-event-3", {key: "something"});
    });
  },
];