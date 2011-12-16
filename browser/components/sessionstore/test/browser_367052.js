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
 * Simon Bünzli <zeniko@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
  /** Test for Bug 367052 **/

  waitForExplicitFinish();
  
  // make sure that the next closed tab will increase getClosedTabCount
  let max_tabs_undo = gPrefService.getIntPref("browser.sessionstore.max_tabs_undo");
  gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", max_tabs_undo + 1);
  let closedTabCount = ss.getClosedTabCount(window);
  
  // restore a blank tab
  let tab = gBrowser.addTab("about:");
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);
    
    let history = tab.linkedBrowser.webNavigation.sessionHistory;
    ok(history.count >= 1, "the new tab does have at least one history entry");
    
    ss.setTabState(tab, JSON.stringify({ entries: [] }));
    tab.linkedBrowser.addEventListener("load", function(aEvent) {
      this.removeEventListener("load", arguments.callee, true);
      ok(history.count == 0, "the tab was restored without any history whatsoever");
      
      gBrowser.removeTab(tab);
      ok(ss.getClosedTabCount(window) == closedTabCount,
         "The closed blank tab wasn't added to Recently Closed Tabs");
      
      // clean up
      if (gPrefService.prefHasUserValue("browser.sessionstore.max_tabs_undo"))
        gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
      finish();
    }, true);
  }, true);
}
