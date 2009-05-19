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
 * The Original Code is Test Code for bug 279022.
 *
 * The Initial Developer of the Original Code is
 * Graeme McCutcheon <graememcc_firefox@graeme-online.co.uk>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

let testPage1 = 'data:text/html,<p>Mozilla</p>';
let testPage2 = 'data:text/html,<p>Firefox</p>';
let testPage3 = 'data:text/html,<p>Seamonkey</p><input type="text" value="Gecko"></input>';
let hButton = gFindBar.getElement("highlight");

var testTab1, testTab2, testTab3;
var testBrowser1, testBrowser2, testBrowser3;

function test() {
  waitForExplicitFinish();

  testTab1 = gBrowser.addTab();
  testTab2 = gBrowser.addTab();
  testTab3 = gBrowser.addTab();
  testBrowser1 = gBrowser.getBrowserForTab(testTab1);
  testBrowser2 = gBrowser.getBrowserForTab(testTab2);
  testBrowser3 = gBrowser.getBrowserForTab(testTab3);

  // Load the test documents into the relevant browsers
  testBrowser3.loadURI(testPage3);
  testBrowser2.loadURI(testPage2);
  testBrowser1.addEventListener("load", testTabs, true);
  testBrowser1.loadURI(testPage1);
}

function testTabs() {
  testBrowser1.removeEventListener("load", testTabs, true);

  // Select the tab and create some highlighting
  gBrowser.selectedTab = testTab1;
  gFindBar.getElement("find-case-sensitive").checked = false;
  gFindBar._highlightDoc(true, "Mozilla");

  // Test 1: switch tab and check highlight button deselected
  gBrowser.selectedTab = testTab2;
  ok(!hButton.checked, "highlight button deselected after changing tab");
  gFindBar._findField.value = "";

  // Test 2: switch back to tab with highlighting
  gBrowser.selectedTab = testTab1;
  ok(hButton.checked, "highlight button re-enabled on tab with highlighting");
  var searchTermOK = gFindBar._findField.value == "Mozilla";
  ok(searchTermOK, "detected correct search term");

  // Create highlighting where match is in an editable element
  gBrowser.selectedTab = testTab3;
  gFindBar._highlightDoc(true, "Gecko");

  // Test 4: Switch to tab without highlighting again
  gBrowser.selectedTab = testTab2;
  ok(!hButton.checked, "highlight button deselected again");
  gFindBar._findField.value = "";

  // Test 5: Switch to tab with highlighting in editable element
  gBrowser.selectedTab = testTab3;
  ok(hButton.checked, "highlighting detected in editable element");
  searchTermOK = gFindBar._findField.value == "Gecko";
  ok(searchTermOK, "detected correct search term");

  // Test 6: Switch between two tabs with highlighting - search term changed?
  gBrowser.selectedTab = testTab1;
  searchTermOK = gFindBar._findField.value == "Mozilla";
  ok(searchTermOK, "correctly changed search term");

  gBrowser.removeTab(testTab3);
  gBrowser.removeTab(testTab2);
  gBrowser.removeTab(testTab1);
  finish();
}

