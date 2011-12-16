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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Thomas de Grenier de Latour <tom.gl@free.fr>
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
  /** Test for Bug 506482 **/

  // test setup
  waitForExplicitFinish();

  // read the sessionstore.js mtime (picked from browser_248970_a.js)
  let profilePath = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties).
                    get("ProfD", Ci.nsIFile);
  function getSessionstoreFile() {
    let sessionStoreJS = profilePath.clone();
    sessionStoreJS.append("sessionstore.js");
    return sessionStoreJS;
  }
  function getSessionstorejsModificationTime() {
    let file = getSessionstoreFile();
    if (file.exists())
      return file.lastModifiedTime;
    else
      return -1;
  }

  // delete existing sessionstore.js, to make sure we're not reading 
  // the mtime of an old one initialy
  let (sessionStoreJS = getSessionstoreFile()) {
    if (sessionStoreJS.exists())
      sessionStoreJS.remove(false);
  }

  // test content URL
  const TEST_URL = "data:text/html,"
    + "<body style='width: 100000px; height: 100000px;'><p>top</p></body>"

  // preferences that we use
  const PREF_INTERVAL = "browser.sessionstore.interval";

  // make sure sessionstore.js is saved ASAP on all events
  gPrefService.setIntPref(PREF_INTERVAL, 0);

  // get the initial sessionstore.js mtime (-1 if it doesn't exist yet)
  let mtime0 = getSessionstorejsModificationTime();

  // create and select a first tab
  let tab = gBrowser.addTab(TEST_URL);
  tab.linkedBrowser.addEventListener("load", function loadListener(e) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    // step1: the above has triggered some saveStateDelayed(), sleep until
    // it's done, and get the initial sessionstore.js mtime
    setTimeout(function step1(e) {
      let mtime1 = getSessionstorejsModificationTime();
      isnot(mtime1, mtime0, "initial sessionstore.js update");

      // step2: test sessionstore.js is not updated on tab selection
      // or content scrolling
      gBrowser.selectedTab = tab;
      tab.linkedBrowser.contentWindow.scrollTo(1100, 1200);
      setTimeout(function step2(e) {
        let mtime2 = getSessionstorejsModificationTime();
        is(mtime2, mtime1, 
           "tab selection and scrolling: sessionstore.js not updated");

        // ok, done, cleanup and finish
        if (gPrefService.prefHasUserValue(PREF_INTERVAL))
          gPrefService.clearUserPref(PREF_INTERVAL);
        gBrowser.removeTab(tab);
        finish();
      }, 3500); // end of sleep after tab selection and scrolling
    }, 3500); // end of sleep after initial saveStateDelayed()
  }, true);
}
