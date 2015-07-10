"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;

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
  function test_background_tab(done) {
    function listener(event, params) {
      is(event, "test-event-background-1", "Correct event name");
      is(params, null, "No param object");
      gContentAPI.observe(null);
      gBrowser.removeCurrentTab();
      done();
    }

    gContentAPI.observe(listener, () => {
      gBrowser.selectedTab = gBrowser.addTab("about:blank");
      isnot(gBrowser.selectedTab, gTestTab, "Make sure the selected tab changed");

      UITour.notify("test-event-background-1");
    });
  },
  // Make sure the tab isn't torn down when switching back to the tour one.
  function test_background_then_foreground_tab(done) {
    let blankTab = null;
    function listener(event, params) {
      is(event, "test-event-4", "Correct event name");
      is(params, null, "No param object");
      gContentAPI.observe(null);
      gBrowser.removeTab(blankTab);
      done();
    }

    gContentAPI.observe(listener, () => {
      blankTab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
      isnot(gBrowser.selectedTab, gTestTab, "Make sure the selected tab changed");
      gBrowser.selectedTab = gTestTab;
      is(gBrowser.selectedTab, gTestTab, "Switch back to the test tab");

      UITour.notify("test-event-4");
    });
  },
];
