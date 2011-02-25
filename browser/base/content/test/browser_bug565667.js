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
 * The Original Code is bug 565667 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul O’Shannessy <paul@oshannessy.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

function test() {
  waitForExplicitFinish();
  // Open the javascript console. It has the mac menu overlay, so browser.js is
  // loaded in it.
  let consoleWin = window.open("chrome://global/content/console.xul", "_blank",
                               "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
  testWithOpenWindow(consoleWin);
}

function testWithOpenWindow(consoleWin) {
  // Add a tab so we don't open the url into the current tab
  let newTab = gBrowser.addTab("http://example.com");
  gBrowser.selectedTab = newTab;

  let numTabs = gBrowser.tabs.length;

  waitForFocus(function() {
    // Sanity check
    is(fm.activeWindow, consoleWin,
       "the console window is focused");

    gBrowser.tabContainer.addEventListener("TabOpen", function(aEvent) {
      gBrowser.tabContainer.removeEventListener("TabOpen", arguments.callee, true);
      let browser = aEvent.originalTarget.linkedBrowser;
      browser.addEventListener("pageshow", function(event) {
        if (event.target.location.href != "about:addons")
          return;
        browser.removeEventListener("pageshow", arguments.callee, true);

        is(fm.activeWindow, window,
           "the browser window was focused");
        is(browser.currentURI.spec, "about:addons",
           "about:addons was loaded in the window");
        is(gBrowser.tabs.length, numTabs + 1,
           "a new tab was added");

        // Cleanup.
        executeSoon(function() {
          consoleWin.close();
          gBrowser.removeTab(gBrowser.selectedTab);
          gBrowser.removeTab(newTab);
          finish();
        });
      }, true);
    }, true);

    // Open the addons manager, uses switchToTabHavingURI.
    consoleWin.BrowserOpenAddonsMgr();
  }, consoleWin);
}

// Ideally we'd also check that the case for no open windows works, but we can't
// due to limitations with the testing framework.
