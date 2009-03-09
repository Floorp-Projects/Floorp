/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This test makes sure that the window title changes correctly while switching
// from and to private browsing mode.

function test() {
  // initialization
  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  const testPageURL = "http://localhost:8888/browser/" +
    "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_windowtitle_page.html";
  waitForExplicitFinish();

  // initialization of expected titles
  let test_title = "Test title";
  let app_name = document.documentElement.getAttribute("title");
  const isOSX = ("nsILocalFileMac" in Ci);
    var page_with_title;
    var page_without_title;
    var pb_page_with_title;
    var pb_page_without_title;
    var about_pb_title;
  if (isOSX) {
    page_with_title = test_title;
    page_without_title = app_name;
    pb_page_with_title = test_title + " - (Private Browsing)";
    pb_page_without_title = app_name + " - (Private Browsing)";
    about_pb_title = pb_page_without_title;
  }
  else {
    page_with_title = test_title + " - " + app_name;
    page_without_title = app_name;
    pb_page_with_title = test_title + " - " + app_name + " (Private Browsing)";
    pb_page_without_title = app_name + " (Private Browsing)";
    about_pb_title = "Private Browsing - " + app_name + " (Private Browsing)";
  }

  // check the window title for a page without a title
  let blankTab = gBrowser.addTab();
  gBrowser.selectedTab = blankTab;
  is(document.title, page_without_title, "The window title for a page without a title matches " +
    "(outside private browsing mode)");
  gBrowser.removeTab(blankTab);

  let pageTab = gBrowser.addTab();
  gBrowser.selectedTab = pageTab;
  let pageBrowser = gBrowser.getBrowserForTab(pageTab);
  pageBrowser.addEventListener("load", function () {
    pageBrowser.removeEventListener("load", arguments.callee, true);

    // check the window title for a page with a title
    is(document.title, page_with_title, "The window title for a page with a title matches " +
      "(outside private browsing mode)");

    gBrowser.removeTab(pageTab);

    // enter the private browsing mode
    pb.privateBrowsingEnabled = true;

    // check the window title for a page without a title
    blankTab = gBrowser.addTab();
    gBrowser.selectedTab = blankTab;
    is(document.title, pb_page_without_title, "The window title for a page without a title matches " +
      "(inside private browsing mode)");
    gBrowser.removeTab(blankTab);

    pageTab = gBrowser.addTab();
    gBrowser.selectedTab = pageTab;
    pageBrowser = gBrowser.getBrowserForTab(pageTab);
    pageBrowser.addEventListener("load", function () {
      pageBrowser.removeEventListener("load", arguments.callee, true);

      // check the window title for a page with a title
      is(document.title, pb_page_with_title, "The window title for a page with a title matches " +
        "(inside private browsing mode)");

      gBrowser.removeTab(pageTab);

      let aboutPBTab = gBrowser.addTab();
      gBrowser.selectedTab = aboutPBTab;
      let aboutPBBrowser = gBrowser.getBrowserForTab(aboutPBTab);
      aboutPBBrowser.addEventListener("load", function() {
        aboutPBBrowser.removeEventListener("load", arguments.callee, true);

        // check the window title for about:privatebrowsing
        is(document.title, about_pb_title, "The window title for about:privatebrowsing matches " +
          "(inside private browsing mode)");

        gBrowser.removeTab(aboutPBTab);

        // cleanup
        pb.privateBrowsingEnabled = false;
        prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
        finish();
      }, true);
      aboutPBBrowser.contentWindow.location = "about:privatebrowsing";
    }, true);
    pageBrowser.contentWindow.location = testPageURL;
  }, true);
  pageBrowser.contentWindow.location = testPageURL;
}
