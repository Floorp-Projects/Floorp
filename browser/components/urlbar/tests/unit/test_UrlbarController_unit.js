/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
// Sinon needs Timer.jsm for setTimeout etc.
ChromeUtils.import("resource://gre/modules/Timer.jsm");
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js", this);
/* globals sinon */
// ================================================

// A fake ProvidersManager.
let fPM;
let sandbox;
let generalListener;
let controller;

/**
 * Asserts that the query context has the expected values.
 *
 * @param {QueryContext} context
 * @param {object} expectedValues The expected values for the QueryContext.
 */
function assertContextMatches(context, expectedValues) {
  Assert.ok(context instanceof QueryContext,
    "Should be a QueryContext");

  for (let [key, value] of Object.entries(expectedValues)) {
    Assert.equal(context[key], value,
      `Should have the expected value for ${key} in the QueryContext`);
  }
}

add_task(function setup() {
  sandbox = sinon.sandbox.create();

  fPM = {
    queryStart: sandbox.stub(),
    queryCancel: sandbox.stub(),
  };

  generalListener = {
    onQueryStarted: sandbox.stub(),
    onQueryResults: sandbox.stub(),
    onQueryCancelled: sandbox.stub(),
  };

  controller = new UrlbarController({
    manager: fPM,
  });
  controller.addQueryListener(generalListener);
});

add_task(function test_add_and_remove_listeners() {
  Assert.throws(() => controller.addQueryListener(null),
    /Expected listener to be an object/,
    "Should throw for a null listener");
  Assert.throws(() => controller.addQueryListener(123),
    /Expected listener to be an object/,
    "Should throw for a non-object listener");

  const listener = {};

  controller.addQueryListener(listener);

  Assert.ok(controller._listeners.has(listener),
    "Should have added the listener to the list.");

  // Adding a non-existent listener shouldn't throw.
  controller.removeQueryListener(123);

  controller.removeQueryListener(listener);

  Assert.ok(!controller._listeners.has(listener),
    "Should have removed the listener from the list");

  sandbox.resetHistory();
});

add_task(function test__notify() {
  const listener1 = {
    onFake: sandbox.stub().callsFake(() => { throw new Error("fake error"); }),
  };
  const listener2 = {
    onFake: sandbox.stub(),
  };

  controller.addQueryListener(listener1);
  controller.addQueryListener(listener2);

  const param = "1234";

  controller._notify("onFake", param);

  Assert.equal(listener1.onFake.callCount, 1,
    "Should have called the first listener method.");
  Assert.deepEqual(listener1.onFake.args[0], [param],
    "Should have called the first listener with the correct argument");
  Assert.equal(listener2.onFake.callCount, 1,
    "Should have called the second listener method.");
  Assert.deepEqual(listener2.onFake.args[0], [param],
    "Should have called the first listener with the correct argument");

  controller.removeQueryListener(listener2);
  controller.removeQueryListener(listener1);

  // This should succeed without errors.
  controller._notify("onNewFake");

  sandbox.resetHistory();
});

add_task(function test_handle_query_starts_search() {
  const context = createContext();
  controller.handleQuery(context);

  Assert.equal(fPM.queryStart.callCount, 1,
    "Should have called queryStart once");
  Assert.equal(fPM.queryStart.args[0].length, 2,
    "Should have called queryStart with two arguments");

  assertContextMatches(fPM.queryStart.args[0][0], {
    autoFill: true,
  });
  Assert.equal(fPM.queryStart.args[0][1], controller,
    "Should have passed the controller as the second argument");


  Assert.equal(generalListener.onQueryStarted.callCount, 1,
    "Should have called onQueryStarted for the listener");
  Assert.deepEqual(generalListener.onQueryStarted.args[0], [context],
    "Should have called onQueryStarted with the context");

  sandbox.resetHistory();
});

add_task(function test_handle_query_starts_search_sets_autoFill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  controller.handleQuery(createContext());

  Assert.equal(fPM.queryStart.callCount, 1,
    "Should have called queryStart once");
  Assert.equal(fPM.queryStart.args[0].length, 2,
    "Should have called queryStart with two arguments");

  assertContextMatches(fPM.queryStart.args[0][0], {
    autoFill: false,
  });
  Assert.equal(fPM.queryStart.args[0][1], controller,
    "Should have passed the controller as the second argument");

  sandbox.resetHistory();

  Services.prefs.clearUserPref("browser.urlbar.autoFill");
});

add_task(function test_cancel_query() {
  const context = createContext();
  controller.cancelQuery(context);

  Assert.equal(fPM.queryCancel.callCount, 1,
    "Should have called queryCancel once");
  Assert.equal(fPM.queryCancel.args[0].length, 1,
    "Should have called queryCancel with one argument");

  Assert.equal(generalListener.onQueryCancelled.callCount, 1,
    "Should have called onQueryCancelled for the listener");
  Assert.deepEqual(generalListener.onQueryCancelled.args[0], [context],
    "Should have called onQueryCancelled with the context");

  sandbox.resetHistory();
});

add_task(function test_receiveResults() {
  const context = createContext();
  controller.receiveResults(context);

  Assert.equal(generalListener.onQueryResults.callCount, 1,
    "Should have called onQueryResults for the listener");
  Assert.deepEqual(generalListener.onQueryResults.args[0], [context],
    "Should have called onQueryResults with the context");

  sandbox.resetHistory();
});
