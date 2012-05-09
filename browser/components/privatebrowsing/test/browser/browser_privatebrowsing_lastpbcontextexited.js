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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function test() {
  // We need to open a new window for this so that its docshell would get destroyed
  // when clearing the PB mode flag.
  let newWin = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  waitForExplicitFinish();
  SimpleTest.waitForFocus(function() {
    let notificationCount = 0;
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        is(aTopic, "last-pb-context-exited", "Correct topic should be dispatched");
        ++notificationCount;
      }
    };
    Services.obs.addObserver(observer, "last-pb-context-exited", false);
    newWin.gPrivateBrowsingUI.privateWindow = true;
    SimpleTest.is(notificationCount, 0, "last-pb-context-exited should not be fired yet");
    newWin.gPrivateBrowsingUI.privateWindow = false;
    newWin.close();
    newWin = null;
    window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils)
          .garbageCollect(); // Make sure that the docshell is destroyed
    SimpleTest.is(notificationCount, 1, "last-pb-context-exited should be fired once");
    Services.obs.removeObserver(observer, "last-pb-context-exited", false);

    // cleanup
    gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
    finish();
  }, newWin);
}
