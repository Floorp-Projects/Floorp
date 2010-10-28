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
 * The Original Code is tabview launch test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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

let tabViewShownCount = 0;

// ----------
function test() {
  waitForExplicitFinish();
  
  // verify initial state
  ok(!TabView.isVisible(), "Tab View starts hidden");

  // use the Tab View button to launch it for the first time
  window.addEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  let button = document.getElementById("tabview-button");
  ok(button, "Tab View button exists");
  button.doCommand();
}

// ----------
function onTabViewLoadedAndShown() {
  window.removeEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  
  ok(TabView.isVisible(), "Tab View is visible. Count: " + tabViewShownCount);
  tabViewShownCount++;
  
  // kick off the series
  window.addEventListener("tabviewshown", onTabViewShown, false);
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  TabView.toggle();
}

// ----------
function onTabViewShown() {
  // add the count to the message so we can track things more easily.
  ok(TabView.isVisible(), "Tab View is visible. Count: " + tabViewShownCount);
  tabViewShownCount++;
  TabView.toggle();
}

// ---------- 
function onTabViewHidden() {
  ok(!TabView.isVisible(), "Tab View is hidden. Count: " + tabViewShownCount);

  if (tabViewShownCount == 1) {
    document.getElementById("menu_tabview").doCommand();
  } else if (tabViewShownCount == 2) {
    EventUtils.synthesizeKey("e", { accelKey: true });
  } else if (tabViewShownCount == 3) {
    window.removeEventListener("tabviewshown", onTabViewShown, false);
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    finish();
  }
}