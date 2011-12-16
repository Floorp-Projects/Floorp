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
 * The Original Code is bug 579868 test.
 *
 * The Initial Developer of the Original Code is
 * Sindre Dammann <sindrebugzilla@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
  let tab1 = gBrowser.addTab("about:robots");
  let tab2 = gBrowser.addTab("about:mozilla");
  tab1.linkedBrowser.addEventListener("load", mainPart, true);
  waitForExplicitFinish();

  function mainPart() {
    tab1.linkedBrowser.removeEventListener("load", mainPart, true);

    // Tell the session storer that the tab is pinned
    let newTabState = '{"entries":[{"url":"about:robots"}],"pinned":true,"userTypedValue":"Hello World!"}';
    ss.setTabState(tab1, newTabState);

    // Undo pinning
    gBrowser.unpinTab(tab1);

    is(tab1.linkedBrowser.__SS_tabStillLoading, true,
       "_tabStillLoading should be true.");

    // Close and restore tab
    gBrowser.removeTab(tab1);
    let savedState = JSON.parse(ss.getClosedTabData(window))[0].state;
    isnot(savedState.pinned, true, "Pinned should not be true");
    tab1 = ss.undoCloseTab(window, 0);

    isnot(tab1.pinned, true, "Should not be pinned");
    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    finish();
  }
}
