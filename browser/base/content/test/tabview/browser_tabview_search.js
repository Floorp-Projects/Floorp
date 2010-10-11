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

// ----------
function test() {
  waitForExplicitFinish();

  // set up our tabs
  let urlBase = "http://mochi.test:8888/browser/browser/base/content/test/tabview/";
  let tabOne = gBrowser.addTab(urlBase + "search1.html");
  let tabTwo = gBrowser.addTab(urlBase + "search2.html");
  newTabs = [ tabOne, tabTwo ];

  // make sure our tabs are loaded so their titles are right
  let stillToLoad = 0; 
  let onLoad = function() {
    this.removeEventListener("load", onLoad, true);
    
    stillToLoad--; 
    if (!stillToLoad) {    
      // show the tab view
      window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
      ok(!TabView.isVisible(), "Tab View is hidden");
      TabView.toggle();
    }
  }
  
  newTabs.forEach(function(tab) {
    stillToLoad++; 
    gBrowser.getBrowserForTab(tab).addEventListener("load", onLoad, true);
  });
}

// ----------
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

// ----------
function searchTest(contentWindow) {
  let searchBox = contentWindow.document.getElementById("searchbox");

  // force an update to make sure the correct titles are in the TabItems
  let tabItems = contentWindow.TabItems.getItems();
  ok(tabItems.length == 3, "Have three tab items");
  tabItems.forEach(function(tabItem) {
    contentWindow.TabItems._update(tabItem.tab);
  });

  // empty string
  searchBox.setAttribute("value", "");
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 0,
     "Match nothing if it's an empty string");

  // one char
  searchBox.setAttribute("value", "s");
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 0,
     "Match nothing if the length of search term is less than 2");

  // the full title
  searchBox.setAttribute("value", "search test 1");
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 1,
     "Match something when the whole title exists");
  
  // part of title
  searchBox.setAttribute("value", "search");
  contentWindow.performSearch();
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 2,
     "Match something when a part of title exists");

  cleanup(contentWindow);
}

// ----------
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
