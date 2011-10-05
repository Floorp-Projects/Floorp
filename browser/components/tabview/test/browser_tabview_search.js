/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTabs = [];

// ----------
function test() {
  waitForExplicitFinish();

  // set up our tabs
  let urlBase = "http://mochi.test:8888/browser/browser/components/tabview/test/";
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
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);

    ok(search.style.display != "none", "Search is enabled");

    let searchBox = contentWindow.document.getElementById("searchbox");
    ok(contentWindow.document.hasFocus() && 
       contentWindow.document.activeElement == searchBox, 
       "The search box has focus");

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
  contentWindow.Search.perform();
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 2,
     "Match something when a part of title exists");

  // unique part of a url 
  searchBox.setAttribute("value", "search1.html");
  contentWindow.Search.perform();
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 1,
     "Match something when a unique part of a url exists");
   
  // common part of a url
  searchBox.setAttribute("value", "tabview");
  contentWindow.Search.perform();
  is(new contentWindow.TabMatcher(
      searchBox.getAttribute("value")).matched().length, 2,
     "Match something when a common part of a url exists");
     
  cleanup(contentWindow);
}

// ----------
function cleanup(contentWindow) {       
  contentWindow.Search.hide(null);     
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
