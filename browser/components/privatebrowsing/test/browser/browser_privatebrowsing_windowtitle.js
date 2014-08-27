/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that the window title changes correctly while switching
// from and to private browsing mode.

function test() {
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
    about_pb_title = "Open a private window?";
    pb_page_with_title = test_title + " - (Private Browsing)";
    pb_page_without_title = app_name + " - (Private Browsing)";
    pb_about_pb_title = pb_page_without_title;
  }
  else {
    page_with_title = test_title + " - " + app_name;
    page_without_title = app_name;
    about_pb_title = "Open a private window?" + " - " + app_name;
    pb_page_with_title = test_title + " - " + app_name + " (Private Browsing)";
    pb_page_without_title = app_name + " (Private Browsing)";
    pb_about_pb_title = "You're browsing privately - " + app_name + " (Private Browsing)";
  }

  function testTabTitle(aWindow, url, insidePB, expected_title, funcNext) {
    executeSoon(function () {
      let tab = aWindow.gBrowser.selectedTab = aWindow.gBrowser.addTab();
      let browser = aWindow.gBrowser.selectedBrowser;
      browser.stop();
      // ensure that the test is run after the titlebar has been updated
      browser.addEventListener("load", function () {
        browser.removeEventListener("load", arguments.callee, true);
        executeSoon(function () {
          if (aWindow.document.title != expected_title) {
            executeSoon(arguments.callee);
            return;
          }
          is(aWindow.document.title, expected_title, "The window title for " + url +
             " is correct (" + (insidePB ? "inside" : "outside") +
             " private browsing mode)");

          let win = aWindow.gBrowser.replaceTabWithWindow(tab);
          win.addEventListener("load", function() {
            win.removeEventListener("load", arguments.callee, false);
            executeSoon(function() {
              if (win.document.title != expected_title) {
                executeSoon(arguments.callee);
                return;
              }
              is(win.document.title, expected_title, "The window title for " + url +
                 " detached tab is correct (" + (insidePB ? "inside" : "outside") +
                 " private browsing mode)");
              win.close();
              aWindow.close();

              setTimeout(funcNext, 0);
            });
          }, false);
        });
      }, true);

      browser.loadURI(url);
    });
  }

  whenNewWindowLoaded({private: false}, function(win) {
    testTabTitle(win, "about:blank", false, page_without_title, function() {
      whenNewWindowLoaded({private: false}, function(win) {
        testTabTitle(win, testPageURL, false, page_with_title, function() {
          whenNewWindowLoaded({private: false}, function(win) {
            testTabTitle(win, "about:privatebrowsing", false, about_pb_title, function() {
              whenNewWindowLoaded({private: true}, function(win) {
                testTabTitle(win, "about:blank", true, pb_page_without_title, function() {
                  whenNewWindowLoaded({private: true}, function(win) {
                    testTabTitle(win, testPageURL, true, pb_page_with_title, function() {
                      whenNewWindowLoaded({private: true}, function(win) {
                        testTabTitle(win, "about:privatebrowsing", true, pb_about_pb_title, finish);
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
  return;
}
