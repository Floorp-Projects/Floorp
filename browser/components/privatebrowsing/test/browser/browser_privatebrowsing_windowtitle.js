/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the window title changes correctly while switching
// from and to private browsing mode.

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const testPageURL = "http://mochi.test:8888/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_windowtitle_page.html";
  waitForExplicitFinish();
  requestLongerTimeout(2);

  // initialization of expected titles
  let test_title = "Test title";
  let app_name = document.documentElement.getAttribute("title");
  const isOSX = ("nsILocalFileMac" in Ci);
  let page_with_title;
  let page_without_title;
  let about_pb_title;
  let pb_page_with_title;
  let pb_page_without_title;
  let pb_about_pb_title;
  if (isOSX) {
    page_with_title = test_title;
    page_without_title = app_name;
    about_pb_title = "Would you like to start Private Browsing?";
    pb_page_with_title = test_title + " - (Private Browsing)";
    pb_page_without_title = app_name + " - (Private Browsing)";
    pb_about_pb_title = pb_page_without_title;
  }
  else {
    page_with_title = test_title + " - " + app_name;
    page_without_title = app_name;
    about_pb_title = "Would you like to start Private Browsing?" + " - " + app_name;
    pb_page_with_title = test_title + " - " + app_name + " (Private Browsing)";
    pb_page_without_title = app_name + " (Private Browsing)";
    pb_about_pb_title = "Private Browsing - " + app_name + " (Private Browsing)";
  }

  function testTabTitle(url, insidePB, expected_title, funcNext) {
    pb.privateBrowsingEnabled = insidePB;

    let tab = gBrowser.selectedTab = gBrowser.addTab();
    let browser = gBrowser.selectedBrowser;
    browser.stop();
    // ensure that the test is run after the titlebar has been updated
    browser.addEventListener("pageshow", function () {
      browser.removeEventListener("pageshow", arguments.callee, false);
      executeSoon(function () {
        is(document.title, expected_title, "The window title for " + url +
           " is correct (" + (insidePB ? "inside" : "outside") +
           " private browsing mode)");

        let win = gBrowser.replaceTabWithWindow(tab);
        win.addEventListener("load", function() {
          win.removeEventListener("load", arguments.callee, false);

          executeSoon(function() {
            is(win.document.title, expected_title, "The window title for " + url +
               " detached tab is correct (" + (insidePB ? "inside" : "outside") +
               " private browsing mode)");
            win.close();

            setTimeout(funcNext, 0);
          });
        }, false);
      });
    }, false);

    browser.loadURI(url);
  }

  function cleanup() {
    pb.privateBrowsingEnabled = false;
    gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
    finish();
  }

  testTabTitle("about:blank", false, page_without_title, function() {
    testTabTitle(testPageURL, false, page_with_title, function() {
      testTabTitle("about:privatebrowsing", false, about_pb_title, function() {
        testTabTitle("about:blank", true, pb_page_without_title, function() {
          testTabTitle(testPageURL, true, pb_page_with_title, function() {
            testTabTitle("about:privatebrowsing", true, pb_about_pb_title, cleanup);
          });
        });
      });
    });
  });
  return;
}
