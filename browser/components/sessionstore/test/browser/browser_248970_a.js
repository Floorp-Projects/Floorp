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
 * The Original Code is sessionstore test code.
 *
 * The Initial Developer of the Original Code is
 * Aaron Train <aaron.train@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

function test() {
  /** Test (A) for Bug 248970 **/

  // test setup
  waitForExplicitFinish();

  // private browsing service
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  function getSessionstorejsModificationTime() {
    // directory service
    let file = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties).
               get("ProfD", Ci.nsIFile);

    // access sessionstore.js
    file.append("sessionstore.js");

    if (file.exists())
      return file.lastModifiedTime;
    else
      return -1;
  }

  // sessionstore service
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);
  let ss_interval = gPrefService.getIntPref("browser.sessionstore.interval");
  let start_mod_time = getSessionstorejsModificationTime();
  gPrefService.setIntPref("browser.sessionstore.interval", 0);
  isnot(start_mod_time, getSessionstorejsModificationTime(),
    "sessionstore.js should be modified when setting the interval to 0");

  //////////////////////////////////////////////////////////////////
  // Test (A) : No data recording while in private browsing mode  //
  //////////////////////////////////////////////////////////////////

  // public session, add a new tab: (A)
  const testURL_A = "http://example.org/";
  let tab_A = gBrowser.addTab(testURL_A);

  tab_A.linkedBrowser.addEventListener("load", function (aEvent) {
    this.removeEventListener("load", arguments.callee, true);

    let prePBModeTimeStamp = getSessionstorejsModificationTime();

    // make sure that the timestamp is bound to differ if sessionstore.js
    // is modified when entering the private mode.
    let timer = Cc["@mozilla.org/timer;1"].
                createInstance(Ci.nsITimer);
    timer.initWithCallback(function (aTimer) {
      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
      ok(pb.privateBrowsingEnabled, "private browsing enabled");

      // sessionstore.js should be modified at this point
      isnot(prePBModeTimeStamp, getSessionstorejsModificationTime(),
        "sessionstore.js should be modified when entering the private browsing mode");

      // record the time stamp of sessionstore.js in the private session
      let startPBModeTimeStamp = getSessionstorejsModificationTime();

      // private browsing session, add new tab: (B)
      const testURL_B = "http://test1.example.org/";
      let tab_B = gBrowser.addTab(testURL_B);

      tab_B.linkedBrowser.addEventListener("load", function (aEvent) {
        this.removeEventListener("load", arguments.callee, true);

        // private browsing session, add new tab: (C)
        const testURL_C = "http://localhost:8888/";
        let tab_C = gBrowser.addTab(testURL_C);

        tab_C.linkedBrowser.addEventListener("load", function (aEvent) {
          this.removeEventListener("load", arguments.callee, true);

          // private browsing session, close tab: (C)
          gBrowser.removeTab(tab_C);

          // private browsing session, close tab: (B)
          gBrowser.removeTab(tab_B);

          // private browsing session, close tab: (A)
          gBrowser.removeTab(tab_A);

          // record the timestamp of sessionstore.js at the end of the private session
          gPrefService.setIntPref("browser.sessionstore.interval", ss_interval);
          gPrefService.setIntPref("browser.sessionstore.interval", 0);
          let endPBModeTimeStamp = getSessionstorejsModificationTime();

          // exit private browsing mode
          pb.privateBrowsingEnabled = false;
          ok(!pb.privateBrowsingEnabled, "private browsing disabled");

          // compare timestamps: pre and post private browsing session
          is(startPBModeTimeStamp, endPBModeTimeStamp,
            "outside private browsing - sessionStore.js timestamp has not changed");

          // cleanup
          gPrefService.setIntPref("browser.sessionstore.interval", ss_interval);
          gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
          finish();
        }, true);
      }, true);
    }, 50, Ci.nsITimer.TYPE_ONE_SHOT);
  }, true);
}
