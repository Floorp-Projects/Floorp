/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test Summary:
// 1.  Open about:sessionrestore via setBrowserState where formdata is a JS object, not a string
// 1a. Check that #sessionData on the page is readable after JSON.parse (skipped, checking formdata is sufficient)
// 1b. Check that there are no backslashes in the formdata
// 1c. Check that formdata (via getBrowserState) doesn't require JSON.parse
//
// 2.  Use the current state (currently about:sessionrestore with data) and then open than in a new instance of about:sessionrestore
// 2a. Check that there are no backslashes in the formdata
// 2b. Check that formdata (via getBrowserState) doesn't require JSON.parse
//
// 3.  [backwards compat] Use a stringified state as formdata when opening about:sessionrestore
// 3a. Make sure there are nodes in the tree on about:sessionrestore (skipped, checking formdata is sufficient)
// 3b. Check that there are no backslashes in the formdata
// 3c. Check that formdata (via getBrowserState) doesn't require JSON.parse

function test() {
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();

  let blankState = { windows: [{ tabs: [{ entries: [{ url: "about:blank" }] }]}]};
  let crashState = { windows: [{ tabs: [{ entries: [{ url: "about:mozilla" }] }]}]};

  let pagedata = { url: "about:sessionrestore",
                   formdata: { id: {"sessionData": crashState } } };
  let state = { windows: [{ tabs: [{ entries: [pagedata] }] }] };

  // test1 calls test2 calls test3 calls finish
  test1(state);


  function test1(aState) {
    waitForBrowserState(aState, function() {
      checkState("test1", test2);
    });
  }

  function test2(aState) {
    let pagedata2 = { url: "about:sessionrestore",
                      formdata: { id: { "sessionData": aState } } };
    let state2 = { windows: [{ tabs: [{ entries: [pagedata2] }] }] };

    waitForBrowserState(state2, function() {
      checkState("test2", test3);
    });
  }

  function test3(aState) {
    let pagedata3 = { url: "about:sessionrestore",
                      formdata: { id: { "sessionData": JSON.stringify(crashState) } } };
    let state3 = { windows: [{ tabs: [{ entries: [pagedata3] }] }] };
    waitForBrowserState(state3, function() {
      // In theory we should do inspection of the treeview on about:sessionrestore,
      // but we don't actually need to. If we fail tests in checkState then
      // about:sessionrestore won't be able to turn the form value into a usable page.
      checkState("test3", function() waitForBrowserState(blankState, finish));
    });
  }

  function checkState(testName, callback) {
    let curState = JSON.parse(ss.getBrowserState());
    let formdata = curState.windows[0].tabs[0].entries[0].formdata;

    ok(formdata.id["sessionData"], testName + ": we have form data for about:sessionrestore");

    let sessionData_raw = JSON.stringify(formdata.id["sessionData"]);
    ok(!/\\/.test(sessionData_raw), testName + ": #sessionData contains no backslashes");
    info(sessionData_raw);

    let gotError = false;
    try {
      JSON.parse(formdata.id["sessionData"]);
    }
    catch (e) {
      info(testName + ": got error: " + e);
      gotError = true;
    }
    ok(gotError, testName + ": attempting to JSON.parse form data threw error");

    // Panorama sticks JSON into extData, which we stringify causing the
    // naive backslash check to fail. extData doesn't matter in the grand
    // scheme here, so we'll delete the extData so doesn't end up in future states.
    delete curState.windows[0].extData;
    delete curState.windows[0].tabs[0].extData;
    callback(curState);
  }

}

