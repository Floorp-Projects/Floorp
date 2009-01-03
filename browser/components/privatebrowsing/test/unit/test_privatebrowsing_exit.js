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

// This test makes sure that the private browsing mode is left at application
// shutdown.

function run_test() {
  // initialization
  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  var pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  var expectedQuitting;
  var called = 0;
  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == kPrivateBrowsingNotification &&
          aData == kExit) {
        // increment the call counter
        ++ called;

        do_check_neq(aSubject, null);
        try {
          aSubject.QueryInterface(Ci.nsISupportsPRBool);
        } catch (ex) {
          do_throw("aSubject was not null, but wasn't an nsISupportsPRBool");
        }
        // check the "quitting" argument
        do_check_eq(aSubject.data, expectedQuitting);

        // finish up the test
        if (expectedQuitting) {
          os.removeObserver(this, kPrivateBrowsingNotification);
          prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
          do_test_finished();
        }
      }
    }
  };

  // set the observer
  os.addObserver(observer, kPrivateBrowsingNotification, false);

  // enter the private browsing mode
  pb.privateBrowsingEnabled = true;

  // exit the private browsing mode
  expectedQuitting = false;
  pb.privateBrowsingEnabled = false;
  do_check_eq(called, 1);

  // enter the private browsing mode
  pb.privateBrowsingEnabled = true;

  // Simulate an exit
  expectedQuitting = true;
  do_test_pending();
  os.notifyObservers(null, "quit-application-granted", null);
}
