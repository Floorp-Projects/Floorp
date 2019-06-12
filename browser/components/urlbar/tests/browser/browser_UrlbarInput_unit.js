/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of UrlbarController by stubbing out the
 * model and providing stubs to be called.
 */

"use strict";

let fakeController;

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

/**
 * Checks the result of a startQuery call on the controller.
 *
 * @param {object} stub The sinon stub that should have been called with the
 *                      UrlbarQueryContext.
 * @param {object} expectedQueryContextProps
 *                   An object consisting of name/value pairs to check against the
 *                   UrlbarQueryContext properties.
 * @param {number} callIndex The expected zero-based index of the call.  Useful
 *                           when startQuery is called multiple times.
 */
function checkStartQueryCall(stub, expectedQueryContextProps, callIndex = 0) {
  Assert.equal(stub.callCount, callIndex + 1,
    "Should have called startQuery on the controller");

  let args = stub.args[callIndex];
  Assert.equal(args.length, 1,
    "Should have called startQuery with one argument");

  let queryContext = args[0];
  Assert.ok(queryContext instanceof UrlbarQueryContext,
    "Should have been passed a UrlbarQueryContext");

  for (let [name, value] of Object.entries(expectedQueryContextProps)) {
    Assert.deepEqual(queryContext[name],
     value, `Should have the correct value for queryContext.${name}`);
  }
}

/**
 * Opens a new empty chrome window and clones the textbox and panel into it.
 *
 * @param {function} callback Called after the window is opened and loaded.
 *                   It's passed a new UrlbarInput object.
 */
async function withNewWindow(callback) {
  // Open a new window, so we don't affect other tests by adding extra
  // UrbarInput wrappers around the urlbar.
  let gTestRoot = getRootDirectory(gTestPath);

  let win = window.openDialog(gTestRoot + "empty.xul",
                    "", "chrome");
  await BrowserTestUtils.waitForEvent(win, "load");

  win.gBrowser = {
    selectedBrowser: {
      getAttribute() {
        return undefined;
      },
    },
  };

  // Clone the elements into the new window, so we get exact copies without having
  // to replicate the xul.
  let doc = win.document;
  let textbox = doc.importNode(document.getElementById("urlbar"), true);
  doc.documentElement.appendChild(textbox);
  let popupset = doc.importNode(document.getElementById("mainPopupSet"), true);
  doc.documentElement.appendChild(popupset);

  let inputOptions = {
    textbox,
    controller: fakeController,
  };

  let input = new UrlbarInput(inputOptions);

  await callback(input);

  await BrowserTestUtils.closeWindow(win);

  // Clean up a bunch of things so we don't leak `win`.
  input.textbox = null;
  input.panel = null;
  input.window = null;
  input.document = null;
  input.view = null;

  Assert.ok(fakeController.view);
  fakeController.removeQueryListener(fakeController.view);
  fakeController.view = null;
}

add_task(async function setup() {
  sandbox = sinon.createSandbox();

  fakeController = new UrlbarController({
    browserWindow: window,
  });

  sandbox.stub(fakeController, "startQuery");
  sandbox.stub(PrivateBrowsingUtils, "isWindowPrivate").returns(false);

  registerCleanupFunction(async () => {
    sandbox.restore();
  });
});

add_task(async function test_input_starts_query() {
  await withNewWindow(input => {
    input.inputField.value = "search";
    input.handleEvent({
      target: input.inputField,
      type: "input",
    });

    checkStartQueryCall(fakeController.startQuery, {
      searchString: "search",
      isPrivate: false,
      maxResults: UrlbarPrefs.get("maxRichResults"),
    });

    sandbox.resetHistory();
  });
});

add_task(async function test_input_with_private_browsing() {
  PrivateBrowsingUtils.isWindowPrivate.returns(true);

  await withNewWindow(privateInput => {
    privateInput.inputField.value = "search";
    privateInput.handleEvent({
      target: privateInput.inputField,
      type: "input",
    });

    checkStartQueryCall(fakeController.startQuery, {
      searchString: "search",
      isPrivate: true,
    });

    sandbox.resetHistory();
  });
});
