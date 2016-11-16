/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {interfaces: Ci, classes: Cc, utils: Cu, manager: Cm, results: Cr} = Components;

Cu.import("resource:///modules/XPCOMUtils.jsm");

function getConsoleMessages() {
  let consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
  let messages = consoleService.getMessageArray().map((m) => m.toString());
  // reset ready for the next call.
  consoleService.reset();
  return messages;
}

function run_test() {
  // Load the component manifests.
  registerXPCTestComponents();
  registerAppManifest(do_get_file('../components/js/xpctest.manifest'));

  // and the tests.
  test_simple();
  test_nested();
}

function test_simple() {
  let parent = Cc["@mozilla.org/js/xpc/test/native/ReturnCodeParent;1"]
               .createInstance(Ci.nsIXPCTestReturnCodeParent);
  let result;

  // flush existing messages before we start testing.
  getConsoleMessages();

  // Ask the C++ to call the JS object which will throw.
  result = parent.callChild(Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_THROW);
  Assert.equal(result, Cr.NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS,
               "exception caused NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS");

  let messages = getConsoleMessages();
  Assert.equal(messages.length, 1, "got a console message from the exception");
  Assert.ok(messages[0].indexOf("a requested error") != -1, "got the message text");

  // Ask the C++ to call the JS object which will return success.
  result = parent.callChild(Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_RETURN_SUCCESS);
  Assert.equal(result, Cr.NS_OK, "success is success");

  Assert.deepEqual(getConsoleMessages(), [], "no messages reported on success.");

  // And finally the point of this test!
  // Ask the C++ to call the JS object which will use .returnCode
  result = parent.callChild(Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_RETURN_RESULTCODE);
  Assert.equal(result, Cr.NS_ERROR_FAILURE,
               "NS_ERROR_FAILURE was seen as the error code.");

  Assert.deepEqual(getConsoleMessages(), [], "no messages reported with .returnCode");
}

function test_nested() {
  let parent = Cc["@mozilla.org/js/xpc/test/native/ReturnCodeParent;1"]
               .createInstance(Ci.nsIXPCTestReturnCodeParent);
  let result;

  // flush existing messages before we start testing.
  getConsoleMessages();

  // Ask the C++ to call the "outer" JS object, which will set .returnCode, but
  // then create and call *another* component which itself sets the .returnCode
  // to a different value.  This checks the returnCode is correctly saved
  // across call contexts.
  result = parent.callChild(Ci.nsIXPCTestReturnCodeChild.CHILD_SHOULD_NEST_RESULTCODES);
  Assert.equal(result, Cr.NS_ERROR_UNEXPECTED,
               "NS_ERROR_UNEXPECTED was seen as the error code.");
  // We expect one message, which is the child reporting what it got as the
  // return code - which should be NS_ERROR_FAILURE
  let expected = ["nested child returned " + Cr.NS_ERROR_FAILURE];
  Assert.deepEqual(getConsoleMessages(), expected, "got the correct sub-error");
}
