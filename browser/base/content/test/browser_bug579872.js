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
 * The Original Code is bug 579872 test.
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
  let newTab = gBrowser.addTab();
  waitForExplicitFinish();
  newTab.linkedBrowser.addEventListener("load", mainPart, true);
  
  function mainPart() {
    gBrowser.pinTab(newTab);
    gBrowser.selectedTab = newTab;
    
    openUILinkIn("javascript:var x=0;", "current");
    is(gBrowser.tabs.length, 2, "Should open in current tab");
    
    openUILinkIn("http://example.com/1", "current");
    is(gBrowser.tabs.length, 2, "Should open in current tab");
    
    openUILinkIn("http://example.org/", "current");
    is(gBrowser.tabs.length, 3, "Should open in new tab");
    
    newTab.removeEventListener("load", mainPart, true);
    gBrowser.removeTab(newTab);
    gBrowser.removeTab(gBrowser.tabs[1]); // example.org tab
    finish();
  }
  newTab.linkedBrowser.loadURI("http://example.com");
}
