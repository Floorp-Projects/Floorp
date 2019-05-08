/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

// A fake ProvidersManager.
let fPM;
let sandbox;
let generalListener;
let controller;

/**
 * Asserts that the query context has the expected values.
 *
 * @param {UrlbarQueryContext} context
 * @param {object} expectedValues The expected values for the UrlbarQueryContext.
 */
function assertContextMatches(context, expectedValues) {
  Assert.ok(context instanceof UrlbarQueryContext,
    "Should be a UrlbarQueryContext");

  for (let [key, value] of Object.entries(expectedValues)) {
    Assert.equal(context[key], value,
      `Should have the expected value for ${key} in the UrlbarQueryContext`);
  }
}

add_task(function setup() {
  sandbox = sinon.createSandbox();

  fPM = {
    startQuery: sandbox.stub(),
    cancelQuery: sandbox.stub(),
  };

  generalListener = {
    onQueryStarted: sandbox.stub(),
    onQueryResults: sandbox.stub(),
    onQueryCancelled: sandbox.stub(),
  };

  controller = new UrlbarController({
    manager: fPM,
    browserWindow: {
      location: {
        href: AppConstants.BROWSER_CHROME_URL,
      },
    },
  });
  controller.addQueryListener(generalListener);
});

add_task(function test_constructor_throws() {
  Assert.throws(() => new UrlbarController(),
    /Missing options: browserWindow/,
    "Should throw if the browserWindow was not supplied");
  Assert.throws(() => new UrlbarController({browserWindow: {}}),
    /browserWindow should be an actual browser window/,
    "Should throw if the browserWindow is not a window");
  Assert.throws(() => new UrlbarController({browserWindow: {
    location: "about:fake",
  }}),
    /browserWindow should be an actual browser window/,
    "Should throw if the browserWindow does not have the correct location");
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
  controller.startQuery(context);

  Assert.equal(fPM.startQuery.callCount, 1,
    "Should have called startQuery once");
  Assert.equal(fPM.startQuery.args[0].length, 2,
    "Should have called startQuery with two arguments");

  assertContextMatches(fPM.startQuery.args[0][0], {});
  Assert.equal(fPM.startQuery.args[0][1], controller,
    "Should have passed the controller as the second argument");

  Assert.equal(generalListener.onQueryStarted.callCount, 1,
    "Should have called onQueryStarted for the listener");
  Assert.deepEqual(generalListener.onQueryStarted.args[0], [context],
    "Should have called onQueryStarted with the context");

  sandbox.resetHistory();
});

add_task(async function test_handle_query_starts_search_sets_allowAutofill() {
  let originalValue = Services.prefs.getBoolPref("browser.urlbar.autoFill");
  Services.prefs.setBoolPref("browser.urlbar.autoFill", !originalValue);

  await controller.startQuery(createContext());

  Assert.equal(fPM.startQuery.callCount, 1,
    "Should have called startQuery once");
  Assert.equal(fPM.startQuery.args[0].length, 2,
    "Should have called startQuery with two arguments");

  assertContextMatches(fPM.startQuery.args[0][0], {
    allowAutofill: !originalValue,
  });
  Assert.equal(fPM.startQuery.args[0][1], controller,
    "Should have passed the controller as the second argument");

  sandbox.resetHistory();

  Services.prefs.clearUserPref("browser.urlbar.autoFill");
});

add_task(function test_cancel_query() {
  const context = createContext();
  controller.startQuery(context);

  controller.cancelQuery();

  Assert.equal(fPM.cancelQuery.callCount, 1,
    "Should have called cancelQuery once");
  Assert.equal(fPM.cancelQuery.args[0].length, 1,
    "Should have called cancelQuery with one argument");

  Assert.equal(generalListener.onQueryCancelled.callCount, 1,
    "Should have called onQueryCancelled for the listener");
  Assert.deepEqual(generalListener.onQueryCancelled.args[0], [context],
    "Should have called onQueryCancelled with the context");

  sandbox.resetHistory();
});

add_task(function test_receiveResults() {
  const context = createContext();
  context.results = [];
  controller.receiveResults(context);

  Assert.equal(generalListener.onQueryResults.callCount, 1,
    "Should have called onQueryResults for the listener");
  Assert.deepEqual(generalListener.onQueryResults.args[0], [context],
    "Should have called onQueryResults with the context");

  sandbox.resetHistory();
});

add_task(async function test_notifications_order() {
  // Clear any pending notifications.
  const context = createContext();
  await controller.startQuery(context);

  // Check that when multiple queries are executed, the notifications arrive
  // in the proper order.
  let collectingListener = new Proxy({}, {
    _notifications: [],
    get(target, name) {
      if (name == "notifications") {
        return this._notifications;
      }
      return () => {
        this._notifications.push(name);
      };
    },
  });
  controller.addQueryListener(collectingListener);
  controller.startQuery(context);
  Assert.deepEqual(["onQueryStarted"], collectingListener.notifications,
                   "Check onQueryStarted is fired synchronously");
  controller.startQuery(context);
  Assert.deepEqual(["onQueryStarted", "onQueryCancelled", "onQueryFinished",
                    "onQueryStarted"],
                   collectingListener.notifications,
                   "Check order of notifications");
  controller.cancelQuery();
  Assert.deepEqual(["onQueryStarted", "onQueryCancelled", "onQueryFinished",
                    "onQueryStarted", "onQueryCancelled", "onQueryFinished"],
                   collectingListener.notifications,
                   "Check order of notifications");
  await controller.startQuery(context);
  controller.cancelQuery();
  Assert.deepEqual(["onQueryStarted", "onQueryCancelled", "onQueryFinished",
                    "onQueryStarted", "onQueryCancelled", "onQueryFinished",
                    "onQueryStarted", "onQueryFinished"],
                   collectingListener.notifications,
                   "Check order of notifications");
});
