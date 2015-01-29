/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Summary:
// 1.  Open about:sessionrestore where formdata is a JS object, not a string
// 1a. Check that #sessionData on the page is readable after JSON.parse (skipped, checking formdata is sufficient)
// 1b. Check that there are no backslashes in the formdata
// 1c. Check that formdata doesn't require JSON.parse
//
// 2.  Use the current state (currently about:sessionrestore with data) and then open that in a new instance of about:sessionrestore
// 2a. Check that there are no backslashes in the formdata
// 2b. Check that formdata doesn't require JSON.parse
//
// 3.  [backwards compat] Use a stringified state as formdata when opening about:sessionrestore
// 3a. Make sure there are nodes in the tree on about:sessionrestore (skipped, checking formdata is sufficient)
// 3b. Check that there are no backslashes in the formdata
// 3c. Check that formdata doesn't require JSON.parse

const CRASH_STATE = {windows: [{tabs: [{entries: [{url: "about:mozilla" }]}]}]};
const STATE = {entries: [createEntry(CRASH_STATE)]};
const STATE2 = {entries: [createEntry({windows: [{tabs: [STATE]}]})]};
const STATE3 = {entries: [createEntry(JSON.stringify(CRASH_STATE))]};

function createEntry(sessionData) {
  return {
    url: "about:sessionrestore",
    formdata: {id: {sessionData: sessionData}}
  };
}

add_task(function test_nested_about_sessionrestore() {
  // Prepare a blank tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // test 1
  yield promiseTabState(tab, STATE);
  checkState("test1", tab);

  // test 2
  yield promiseTabState(tab, STATE2);
  checkState("test2", tab);

  // test 3
  yield promiseTabState(tab, STATE3);
  checkState("test3", tab);

  // Cleanup.
  gBrowser.removeTab(tab);
});

function checkState(prefix, tab) {
  // Flush and query tab state.
  TabState.flush(tab.linkedBrowser);
  let {formdata} = JSON.parse(ss.getTabState(tab));

  ok(formdata.id["sessionData"], prefix + ": we have form data for about:sessionrestore");

  let sessionData_raw = JSON.stringify(formdata.id["sessionData"]);
  ok(!/\\/.test(sessionData_raw), prefix + ": #sessionData contains no backslashes");
  info(sessionData_raw);

  let gotError = false;
  try {
    JSON.parse(formdata.id["sessionData"]);
  } catch (e) {
    info(prefix + ": got error: " + e);
    gotError = true;
  }
  ok(gotError, prefix + ": attempting to JSON.parse form data threw error");
}
