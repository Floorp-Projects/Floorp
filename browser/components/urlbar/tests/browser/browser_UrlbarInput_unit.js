/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

let fakeController;
let generalListener;
let input;
let inputOptions;

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
 * Checks the result of a startQuery call on the controller.
 *
 * @param {object} stub The sinon stub that should have been called with the
 *                      QueryContext.
 * @param {object} expectedQueryContextProps
 *                   An object consisting of name/value pairs to check against the
 *                   QueryContext properties.
 */
function checkStartQueryCall(stub, expectedQueryContextProps) {
  Assert.equal(stub.callCount, 1,
    "Should have called startQuery on the controller");

  let args = stub.args[0];
  Assert.equal(args.length, 1,
    "Should have called startQuery with one argument");

  let queryContext = args[0];
  Assert.ok(queryContext instanceof QueryContext,
    "Should have been passed a QueryContext");

  for (let [name, value] of Object.entries(expectedQueryContextProps)) {
    Assert.deepEqual(queryContext[name],
     value, `Should have the correct value for queryContext.${name}`);
  }
}

add_task(async function setup() {
  sandbox = sinon.sandbox.create();

  fakeController = new UrlbarController({
    browserWindow: window,
  });

  sandbox.stub(fakeController, "startQuery");
  sandbox.stub(PrivateBrowsingUtils, "isWindowPrivate").returns(false);

  // Open a new window, so we don't affect other tests by adding extra
  // UrbarInput wrappers around the urlbar.
  let gTestRoot = getRootDirectory(gTestPath);

  let win = window.openDialog(gTestRoot + "empty.xul",
                    "", "chrome");
  await BrowserTestUtils.waitForEvent(win, "load");

  win.gBrowser = {};

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
    sandbox.restore();
  });

  // Clone the elements into the new window, so we get exact copies without having
  // to replicate the xul.
  let doc = win.document;
  let textbox = doc.importNode(document.getElementById("urlbar"), true);
  doc.documentElement.appendChild(textbox);
  let panel = doc.importNode(document.getElementById("urlbar-results"), true);
  doc.documentElement.appendChild(panel);

  inputOptions = {
    textbox,
    panel,
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

  checkStartQueryCall(fakeController.startQuery, {
    searchString: "search",
    isPrivate: false,
    maxResults: UrlbarPrefs.get("maxRichResults"),
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

  checkStartQueryCall(fakeController.startQuery, {
    searchString: "search",
    isPrivate: true,
  });

  sandbox.resetHistory();
});
