/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

let fakeController;
let sandbox;
let generalListener;
let input;
let inputOptions;

ChromeUtils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

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

/**
 * @returns {object} A fake element with minimal functions for simulating textbox etc.
 */
function createFakeElement() {
  return {
    addEventListener() {},
  };
}

/**
 * Checks the result of a handleQuery call on the controller.
 *
 * @param {object} stub The sinon stub that should have been called with the
 *                      QueryContext.
 * @param {object} expectedQueryContextProps
 *                   An object consisting of name/value pairs to check against the
 *                   QueryContext properties.
 */
function checkHandleQueryCall(stub, expectedQueryContextProps) {
  Assert.equal(stub.callCount, 1,
    "Should have called handleQuery on the controller");

  let args = stub.args[0];
  Assert.equal(args.length, 1,
    "Should have called handleQuery with one argument");

  let queryContext = args[0];
  Assert.ok(queryContext instanceof QueryContext,
    "Should have been passed a QueryContext");

  for (let [name, value] of Object.entries(expectedQueryContextProps)) {
    Assert.deepEqual(queryContext[name],
     value, `Should have the correct value for queryContext.${name}`);
  }
}

add_task(function setup() {
  sandbox = sinon.sandbox.create();

  fakeController = new UrlbarController();

  sandbox.stub(fakeController, "handleQuery");
  sandbox.stub(PrivateBrowsingUtils, "isWindowPrivate").returns(false);

  let textbox = createFakeElement();
  textbox.inputField = createFakeElement();
  textbox.inputField.controllers = { insertControllerAt() {} };
  inputOptions = {
    textbox,
    panel: {
      ownerDocument: {},
      querySelector() {
        return createFakeElement();
      },
    },
    controller: fakeController,
  };

  input = new UrlbarInput(inputOptions);
});

add_task(function test_input_starts_query() {
  input.handleEvent({
    target: {
      value: "search",
    },
    type: "input",
  });

  checkHandleQueryCall(fakeController.handleQuery, {
    searchString: "search",
    isPrivate: false,
  });

  sandbox.resetHistory();
});

add_task(function test_input_with_private_browsing() {
  PrivateBrowsingUtils.isWindowPrivate.returns(true);

  // Rather than using the global input here, we create a new instance which
  // will use the updated return value of the private browsing stub.
  let privateInput = new UrlbarInput(inputOptions);

  privateInput.handleEvent({
    target: {
      value: "search",
    },
    type: "input",
  });

  checkHandleQueryCall(fakeController.handleQuery, {
    searchString: "search",
    isPrivate: true,
  });

  sandbox.resetHistory();
});
