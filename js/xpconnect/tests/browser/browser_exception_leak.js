/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// For bug 1471989, test that an exception saved by chrome code can't leak the page.

add_task(async function test() {
  const url = "http://mochi.test:8888/browser/js/xpconnect/tests/browser/browser_consoleStack.html";
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;
  let innerWindowId = browser.innerWindowID;

  let stackTraceEmpty = await ContentTask.spawn(browser, {innerWindowId}, async function(args) {
    let {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm");
    let {Assert} = ChromeUtils.import("resource://testing-common/Assert.jsm");

    const ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(Ci.nsIConsoleAPIStorage);
    let consoleEvents = ConsoleAPIStorage.getEvents(args.innerWindowId);
    Assert.equal(consoleEvents.length, 1, "Should only be one console event for the window");

    // Intentionally hold a reference to the console event.
    let leakedConsoleEvent = consoleEvents[0];

    let doc = content.document;
    let promise = TestUtils.topicObserved("inner-window-nuked", (subject, data) => {
      let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      return id == args.innerWindowId;
    });
    content.location = "http://mochi.test:8888/";
    await promise;

    // This string should be empty. For that to happen, two things
    // need to be true:
    //
    // a) ConsoleCallData::mStack is not null. This means that the
    // stack trace was not reified before the page was nuked. If it
    // was, then the correct |filename| value would be stored on the
    // object. (This is not a problem, except that it stops us from
    // testing the next condition.)
    //
    // b) ConsoleData::mStack.mStack is null. This means that the
    // JSStackFrame is keeping alive the JS object in the page after
    // the page was nuked, which leaks the page.
    return leakedConsoleEvent.stacktrace[0].filename;
  });

  is(stackTraceEmpty, "",
     "JSStackFrame shouldn't leak mStack after window nuking");

  BrowserTestUtils.removeTab(newTab);
});
