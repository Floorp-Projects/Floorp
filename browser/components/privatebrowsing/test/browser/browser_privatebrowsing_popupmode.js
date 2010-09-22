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

// This test makes sure that the URL bar is enabled after entering the private
// browsing mode, even if it's disabled before that (see bug 495495).

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  let ss = Cc["@mozilla.org/browser/sessionstore;1"].
           getService(Ci.nsISessionStore);

  // clear the history of closed windows (that other tests have created)
  // to avoid the issue in bug 596592
  // XXX remove this when bug 597071 is fixed
  while (ss.getClosedWindowCount())
    ss.forgetClosedWindow(0);

  // backup our state
  let stateBackup = ss.getWindowState(window);

  function pretendToBeAPopup(whatToPretend) {
    let state = whatToPretend ?
      '{"windows":[{"tabs":[{"entries":[],"attributes":{}}],"isPopup":true,"hidden":"toolbar"}]}' :
      '{"windows":[{"tabs":[{"entries":[],"attributes":{}}],"isPopup":false}]}';
    ss.setWindowState(window, state, true);
    if (whatToPretend) {
      ok(gURLBar.readOnly, "pretendToBeAPopup correctly made the URL bar read-only");
      is(gURLBar.getAttribute("enablehistory"), "false",
         "pretendToBeAPopup correctly disabled autocomplete on the URL bar");
    }
    else {
      ok(!gURLBar.readOnly, "pretendToBeAPopup correctly made the URL bar read-write");
      is(gURLBar.getAttribute("enablehistory"), "true",
         "pretendToBeAPopup correctly enabled autocomplete on the URL bar");
    }
  }

  // first, test the case of entering the PB mode when a popup window is active

  // pretend we're a popup window
  pretendToBeAPopup(true);

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  // make sure that the url bar status is correctly restored
  ok(!gURLBar.readOnly,
     "URL bar should not be read-only after entering the private browsing mode");
  is(gURLBar.getAttribute("enablehistory"), "true",
     "URL bar autocomplete should be enabled after entering the private browsing mode");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  // we're no longer a popup window
  pretendToBeAPopup(false);

  // then, test the case of leaving the PB mode when a popup window is active

  // start from within the private browsing mode
  pb.privateBrowsingEnabled = true;

  // pretend we're a popup window
  pretendToBeAPopup(true);

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  // make sure that the url bar status is correctly restored
  ok(!gURLBar.readOnly,
     "URL bar should not be read-only after leaving the private browsing mode");
  is(gURLBar.getAttribute("enablehistory"), "true",
     "URL bar autocomplete should be enabled after leaving the private browsing mode");

  // cleanup
  ss.setWindowState(window, stateBackup, true);
}
