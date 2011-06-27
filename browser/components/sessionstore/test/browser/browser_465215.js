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
  /** Test for Bug 465215 **/
  
  waitForExplicitFinish();
  
  let uniqueName = "bug 465215";
  let uniqueValue1 = "as good as unique: " + Date.now();
  let uniqueValue2 = "as good as unique: " + Math.random();
  
  // set a unique value on a new, blank tab
  let tab1 = gBrowser.addTab();
  tab1.linkedBrowser.addEventListener("load", function() {
    tab1.linkedBrowser.removeEventListener("load", arguments.callee, true);
    ss.setTabValue(tab1, uniqueName, uniqueValue1);
    
    // duplicate the tab with that value
    let tab2 = ss.duplicateTab(window, tab1);
    is(ss.getTabValue(tab2, uniqueName), uniqueValue1, "tab value was duplicated");
    
    ss.setTabValue(tab2, uniqueName, uniqueValue2);
    isnot(ss.getTabValue(tab1, uniqueName), uniqueValue2, "tab values aren't sync'd");
    
    // overwrite the tab with the value which should remove it
    ss.setTabState(tab1, JSON.stringify({ entries: [] }));
    tab1.linkedBrowser.addEventListener("load", function() {
      tab1.linkedBrowser.removeEventListener("load", arguments.callee, true);
      is(ss.getTabValue(tab1, uniqueName), "", "tab value was cleared");
      
      // clean up
      gBrowser.removeTab(tab2);
      gBrowser.removeTab(tab1);
      finish();
    }, true);
  }, true);
}
