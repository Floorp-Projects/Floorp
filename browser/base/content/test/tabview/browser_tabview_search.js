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
 * The Original Code is tabview search test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
 * Sean Dunn <seanedunn@yahoo.com>
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

let newTabs = [];

function test() {
  waitForExplicitFinish();

  let tabOne = gBrowser.addTab();
  let tabTwo = gBrowser.addTab("http://mochi.test:8888/");

  let browser = gBrowser.getBrowserForTab(tabTwo);
  let onLoad = function() {
    browser.removeEventListener("load", onLoad, true);
    
    // show the tab view
    window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
    ok(!TabView.isVisible(), "Tab View is hidden");
    TabView.toggle();
  }
  browser.addEventListener("load", onLoad, true);
  newTabs = [ tabOne, tabTwo ];
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let search = contentWindow.document.getElementById("search");
  let searchButton = contentWindow.document.getElementById("searchbutton");

  ok(searchButton, "Search button exists");
  
  let onSearchEnabled = function() {
    ok(search.style.display != "none", "Search is enabled");
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);
    searchTest(contentWindow);
  }
  contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, false);
  // enter search mode
  EventUtils.sendMouseEvent({ type: "mousedown" }, searchButton, contentWindow);
}

function searchTest(contentWindow) {
  let searchBox = contentWindow.document.getElementById("searchbox");

  // get the titles of tabs.
  let tabNames = [];
  let tabItems = contentWindow.TabItems.getItems();

  ok(tabItems.length == 3, "Have three tab items");
  
  tabItems.forEach(function(tab) {
    tabNames.push(tab.nameEl.innerHTML);
  });
  ok(tabNames[0] && tabNames[0].length > 2, 
     "The title of tab item is longer than 2 chars")

  // empty string
  searchBox.setAttribute("value", "");
  ok(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length == 0,
     "Match nothing if it's an empty string");

  // one char
  searchBox.setAttribute("value", tabNames[0].charAt(0));
  ok(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length == 0,
     "Match nothing if the length of search term is less than 2");

  // the full title
  searchBox.setAttribute("value", tabNames[2]);
  ok(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length == 1,
     "Match something when the whole title exists");
  
  // part of titled
  searchBox.setAttribute("value", tabNames[0].substr(1));
  contentWindow.performSearch();
  ok(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length == 2,
     "Match something when a part of title exists");

  cleanup(contentWindow);
}

function cleanup(contentWindow) {       
  contentWindow.hideSearch(null);     
  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);
    ok(!TabView.isVisible(), "Tab View is hidden");

    gBrowser.removeTab(newTabs[0]);
    gBrowser.removeTab(newTabs[1]);

    finish();
  }
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  EventUtils.synthesizeKey("VK_ENTER", {});
}
