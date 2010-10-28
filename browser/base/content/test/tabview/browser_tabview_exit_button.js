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
 * The Original Code is tabview exit button test.
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

let contentWindow;
let appTab;
let originalTab;
let exitButton;

// ----------
function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  TabView.toggle();
}

// ----------
function onTabViewLoadedAndShown() {
  window.removeEventListener("tabviewshown", onTabViewLoadedAndShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  contentWindow = document.getElementById("tab-view").contentWindow;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1,
      "we start with one group (the default)");
  is(gBrowser.tabs.length, 1, "we start with one tab");
  originalTab = gBrowser.tabs[0];
  ok(!originalTab.pinned, "the original tab is not an app tab");

  // create an app tab
  appTab = gBrowser.loadOneTab("about:blank");
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  gBrowser.pinTab(appTab);

  // verify that the normal tab is selected
  ok(gBrowser.selectedTab == originalTab, "the normal tab is selected");

  // hit the exit button for the first time
  exitButton = contentWindow.document.getElementById("exit-button");
  ok(exitButton, "Exit button exists");

  window.addEventListener("tabviewhidden", onTabViewHiddenForNormalTab, false);
  EventUtils.sendMouseEvent({ type: "click" }, exitButton, contentWindow);
}

// ----------
function onTabViewHiddenForNormalTab() {
  window.removeEventListener("tabviewhidden", onTabViewHiddenForNormalTab, false);
  ok(!TabView.isVisible(), "Tab View is not visible");

  // verify that the normal tab is still selected
  ok(gBrowser.selectedTab == originalTab, "the normal tab is still selected");

  // select the app tab
  gBrowser.selectedTab = appTab;
  ok(gBrowser.selectedTab == appTab, "the app tab is now selected");

  // go back to tabview
  window.addEventListener("tabviewshown", onTabViewShown, false);
  TabView.toggle();
}

// ----------
function onTabViewShown() {
  window.removeEventListener("tabviewshown", onTabViewShown, false);
  ok(TabView.isVisible(), "Tab View is visible");

  // hit the exit button again
  window.addEventListener("tabviewhidden", onTabViewHiddenForAppTab, false);
  EventUtils.sendMouseEvent({ type: "click" }, exitButton, contentWindow);
}

// ----------
function onTabViewHiddenForAppTab() {
  window.removeEventListener("tabviewhidden", onTabViewHiddenForAppTab, false);
  ok(!TabView.isVisible(), "Tab View is not visible");

  // verify that the app tab is still selected
  ok(gBrowser.selectedTab == appTab, "the app tab is still selected");

  // clean up
  gBrowser.selectedTab = originalTab; 
  gBrowser.removeTab(appTab);

  is(gBrowser.tabs.length, 1, "we finish with one tab");
  ok(gBrowser.selectedTab == originalTab,
      "we finish with the normal tab selected");
  ok(!TabView.isVisible(), "we finish with Tab View not visible");

  finish();
}
