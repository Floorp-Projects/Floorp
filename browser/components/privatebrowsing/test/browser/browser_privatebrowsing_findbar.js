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

// This test makes sure that the find bar is cleared when leaving the
// private browsing mode.

function test() {
  // initialization
  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  // fill in the find bar with something
  const kTestSearchString = "privatebrowsing";
  let findBox = gFindBar.getElement("findbar-textbox");
  gFindBar.startFind(gFindBar.FIND_NORMAL);
  for (let i = 0; i < kTestSearchString.length; ++ i)
    EventUtils.synthesizeKey(kTestSearchString[i], {});

  // enter private browsing mode
  pb.privateBrowsingEnabled = true;

  is(findBox.value, kTestSearchString,
    "entering the private browsing mode should not clear the findbar");
  ok(findBox.editor.transactionManager.numberOfUndoItems > 0,
    "entering the private browsing mode should not reset the undo list of the findbar control");

  // leave private browsing mode
  pb.privateBrowsingEnabled = false;

  is(findBox.value, "",
    "leaving the private browsing mode should clear the findbar");
  is(findBox.editor.transactionManager.numberOfUndoItems, 0,
    "leaving the private browsing mode should reset the undo list of the findbar control");

  // cleanup
  prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  gFindBar.close();
}
