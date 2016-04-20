/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
  We have three ways for content to open new windows:
     1) window.open (with the default features)
     2) window.open (with non-default features)
     3) target="_blank" in <a> tags

     We also have two prefs that modify our window opening behaviours:

     1) browser.link.open_newwindow

        This has a numeric value that allows us to set our window-opening behaviours from
        content in three ways:
        1) Open links that would normally open a new window in the current tab
        2) Open links that would normally open a new window in a new window
        3) Open links that would normally open a new window in a new tab (default)

     2) browser.link.open_newwindow.restriction

        This has a numeric value that allows us to fine tune the browser.link.open_newwindow
        pref so that it can discriminate between different techniques for opening windows.

        0) All things that open windows should behave according to browser.link.open_newwindow.
        1) No things that open windows should behave according to browser.link.open_newwindow
           (essentially rendering browser.link.open_newwindow inert).
        2) Most things that open windows should behave according to browser.link.open_newwindow,
           _except_ for window.open calls with the "feature" parameter. This will open in a new
           window regardless of what browser.link.open_newwindow is set at. (default)

     This file attempts to test each window opening technique against all possible settings for
     each preference.
*/

Cu.import("resource://gre/modules/Task.jsm");

const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kContentDoc = "http://www.example.com/browser/dom/tests/browser/test_new_window_from_content_child.html";
const kNewWindowPrefKey = "browser.link.open_newwindow";
const kNewWindowRestrictionPrefKey = "browser.link.open_newwindow.restriction";
const kSameTab = "same tab";
const kNewWin = "new window";
const kNewTab = "new tab";

SpecialPowers.pushPrefEnv({"set": [["dom.require_user_interaction_for_beforeunload", false]]});

requestLongerTimeout(3);

// The following "matrices" represent the result of content attempting to
// open a window with window.open with the default feature set. The key of
// the kWinOpenDefault object represents the value of browser.link.open_newwindow.
// The value for each key is an array that represents the result (either opening
// the link in the same tab, a new window, or a new tab), where the index of each
// result maps to the browser.link.open_newwindow.restriction pref. I've tried
// to illustrate this more clearly in the kWinOpenDefault object.
const kWinOpenDefault = {
//                    open_newwindow.restriction
//                    0         1        2
// open_newwindow
                  1: [kSameTab, kNewWin, kSameTab],
                  2: [kNewWin, kNewWin, kNewWin],
                  3: [kNewTab, kNewWin, kNewTab],
};

const kWinOpenNonDefault = {
  1: [kSameTab, kNewWin, kNewWin],
  2: [kNewWin, kNewWin, kNewWin],
  3: [kNewTab, kNewWin, kNewWin],
};

const kTargetBlank = {
  1: [kSameTab, kSameTab, kSameTab],
  2: [kNewWin, kNewWin, kNewWin],
  3: [kNewTab, kNewTab, kNewTab],
};

// We'll be changing these preferences a lot, so we'll stash their original
// values and make sure we restore them at the end of the test.
var originalNewWindowPref = Services.prefs.getIntPref(kNewWindowPrefKey);
var originalNewWindowRestrictionPref =
  Services.prefs.getIntPref(kNewWindowRestrictionPrefKey);

registerCleanupFunction(function() {
  Services.prefs.setIntPref(kNewWindowPrefKey, originalNewWindowPref);
  Services.prefs.setIntPref(kNewWindowRestrictionPrefKey,
                            originalNewWindowRestrictionPref);
});

/**
 * For some expectation when a link is clicked, creates and
 * returns a Promise that resolves when that expectation is
 * fulfilled. For example, aExpectation might be kSameTab, which
 * will cause this function to return a Promise that resolves when
 * the current tab attempts to browse to about:blank.
 *
 * This function also takes care of cleaning up once the result has
 * occurred - for example, if a new window was opened, this function
 * closes it before resolving.
 *
 * @param aBrowser the <xul:browser> with the test document
 * @param aExpectation one of kSameTab, kNewWin, or kNewTab.
 * @return a Promise that resolves when the expectation is fulfilled,
 *         and cleaned up after.
 */
function prepareForResult(aBrowser, aExpectation) {
  let expectedSpec = kContentDoc.replace(/[^\/]*$/, "dummy.html");
  switch(aExpectation) {
    case kSameTab:
      return Task.spawn(function*() {
        yield BrowserTestUtils.browserLoaded(aBrowser);
        is(aBrowser.currentURI.spec, expectedSpec, "Should be at dummy.html");
        // Now put the browser back where it came from
        yield BrowserTestUtils.loadURI(aBrowser, kContentDoc);
        yield BrowserTestUtils.browserLoaded(aBrowser);
      });
      break;
    case kNewWin:
      return Task.spawn(function*() {
        let newWin = yield BrowserTestUtils.waitForNewWindow();
        let newBrowser = newWin.gBrowser.selectedBrowser;
        yield BrowserTestUtils.browserLoaded(newBrowser);
        is(newBrowser.currentURI.spec, expectedSpec, "Should be at dummy.html");
        yield BrowserTestUtils.closeWindow(newWin);
      });
      break;
    case kNewTab:
      return Task.spawn(function*() {
        let newTab = yield BrowserTestUtils.waitForNewTab(gBrowser);
        is(newTab.linkedBrowser.currentURI.spec, expectedSpec,
           "Should be at dummy.html");
        yield BrowserTestUtils.removeTab(newTab);
      });
      break;
    default:
      ok(false, "prepareForResult can't handle an expectation of " + aExpectation)
      return;
  }

  return deferred.promise;
}

/**
 * Ensure that clicks on a link with ID aLinkID cause us to
 * perform as specified in the supplied aMatrix (kWinOpenDefault,
 * for example).
 *
 * @param aLinkSelector a selector for the link within the testing page to click.
 * @param aMatrix a testing matrix for the
 *        browser.link.open_newwindow and browser.link.open_newwindow.restriction
 *        prefs to test against. See kWinOpenDefault for an example.
 */
function testLinkWithMatrix(aLinkSelector, aMatrix) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: kContentDoc,
  }, function*(browser) {
    // This nested for-loop is unravelling the matrix const
    // we set up, and gives us three things through each tick
    // of the inner loop:
    // 1) newWindowPref: a browser.link.open_newwindow pref to try
    // 2) newWindowRestPref: a browser.link.open_newwindow.restriction pref to try
    // 3) expectation: what we expect the click outcome on this link to be,
    //                 which will either be kSameTab, kNewWin or kNewTab.
    for (let newWindowPref in aMatrix) {
      let expectations = aMatrix[newWindowPref];
      for (let i = 0; i < expectations.length; ++i) {
        let newWindowRestPref = i;
        let expectation = expectations[i];

        Services.prefs.setIntPref("browser.link.open_newwindow", newWindowPref);
        Services.prefs.setIntPref("browser.link.open_newwindow.restriction", newWindowRestPref);
        info("Clicking on " + aLinkSelector);
        info("Testing with browser.link.open_newwindow = " + newWindowPref + " and " +
             "browser.link.open_newwindow.restriction = " + newWindowRestPref);
        info("Expecting: " + expectation);
        let resultPromise = prepareForResult(browser, expectation);
        BrowserTestUtils.synthesizeMouseAtCenter(aLinkSelector, {}, browser);
        yield resultPromise;
        info("Got expectation: " + expectation);
      }
    }
  });
}

add_task(function* test_window_open_with_defaults() {
  yield testLinkWithMatrix("#winOpenDefault", kWinOpenDefault);
});

add_task(function* test_window_open_with_non_defaults() {
  yield testLinkWithMatrix("#winOpenNonDefault", kWinOpenNonDefault);
});

add_task(function* test_window_open_dialog() {
  yield testLinkWithMatrix("#winOpenDialog", kWinOpenNonDefault);
});

add_task(function* test_target__blank() {
  yield testLinkWithMatrix("#targetBlank", kTargetBlank);
});
