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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

// This test makes sure that the page info dialogs will close when entering or
// exiting the private browsing mode.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);

  function runTest(aPBMode, aCallBack) {
    const kTestURL1 = "data:text/html,Test Page 1";
    let tab1 = gBrowser.addTab();
    gBrowser.selectedTab = tab1;
    let browser1 = gBrowser.getBrowserForTab(tab1);
    browser1.addEventListener("load", function() {
      browser1.removeEventListener("load", arguments.callee, true);

      let pageInfo1 = BrowserPageInfo();
      obs.addObserver({
        observe: function (aSubject, aTopic, aData) {
          obs.removeObserver(this, "page-info-dialog-loaded");

          const kTestURL2 = "data:text/plain,Test Page 2";
          let tab2 = gBrowser.addTab();
          gBrowser.selectedTab = tab2;
          let browser2 = gBrowser.getBrowserForTab(tab2);
          browser2.addEventListener("load", function () {
            browser2.removeEventListener("load", arguments.callee, true);

            let pageInfo2 = BrowserPageInfo();
            obs.addObserver({
              observe: function (aSubject, aTopic, aData) {
                obs.removeObserver(this, "page-info-dialog-loaded");

                ww.registerNotification({
                  observe: function (aSubject, aTopic, aData) {
                    is(aTopic, "domwindowclosed", "We should only receive window closed notifications");
                    let win = aSubject.QueryInterface(Ci.nsIDOMWindow);
                    if (win == pageInfo1) {
                      ok(true, "Page info 1 being closed as expected");
                      pageInfo1 = null;
                    }
                    else if (win == pageInfo2) {
                      ok(true, "Page info 2 being closed as expected");
                      pageInfo2 = null;
                    }
                    else
                      ok(false, "The closed window should be one of the two page info windows");

                    if (!pageInfo1 && !pageInfo2) {
                      ww.unregisterNotification(this);

                      aCallBack();
                    }
                  }
                });

                pb.privateBrowsingEnabled = aPBMode;
              }
            }, "page-info-dialog-loaded", false);
          }, true);
          browser2.loadURI(kTestURL2);
        }
      }, "page-info-dialog-loaded", false);
    }, true);
    browser1.loadURI(kTestURL1);
  }

  runTest(true, function() {
    runTest(false, function() {
      gBrowser.removeCurrentTab();
      gBrowser.removeCurrentTab();

      finish();
    });
  });

  waitForExplicitFinish();
}
