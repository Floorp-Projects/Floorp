/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newWindows = [];

function test() {
  waitForExplicitFinish();
  let windowOne = openDialog(location, "", "chrome,all,dialog=no", "data:text/html,");
  let windowTwo;

  whenWindowLoaded(windowOne, function () {
    windowOne.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      windowOne.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      windowTwo = openDialog(location, "", "chrome,all,dialog=no", "http://mochi.test:8888/");
      whenWindowLoaded(windowTwo, function () {
        windowTwo.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
          windowTwo.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

          newWindows = [ windowOne, windowTwo ];

          // show the tab view
          window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
          ok(!TabView.isVisible(), "Tab View is hidden");
          TabView.toggle();
        }, true);
      });
    }, true);
  });
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

// conveniently combine local and other window tab results from a query
function getMatchResults(contentWindow, query) {
  let matcher = new contentWindow.TabMatcher(query);
  let localMatchResults = matcher.matched();
  let otherMatchResults = matcher.matchedTabsFromOtherWindows();
  return localMatchResults.concat(otherMatchResults);
}

function searchTest(contentWindow) {
  let searchBox = contentWindow.document.getElementById("searchbox");
  let matcher = null;
  let matchResults = [];
  
  // get the titles of tabs.
  let tabNames = [];

  let tabItems = contentWindow.TabItems.getItems();
  is(tabItems.length, 1, "Have only one tab in the current window's tab items"); 
  tabItems.forEach(function(tab) {
    tabNames.push(tab.$tabTitle[0].innerHTML);
  });

  newWindows.forEach(function(win) {
      for(var i=0; i<win.gBrowser.tabs.length; ++i) {
        tabNames.push(win.gBrowser.tabs[i].label);
      }
  });

  ok(tabNames[0] && tabNames[0].length > 2, 
     "The title of tab item is longer than 2 chars")

  // empty string
  searchBox.setAttribute("value", "");
  matchResults = getMatchResults(contentWindow, searchBox.getAttribute("value"));
  ok(matchResults.length == 0, "Match nothing if it's an empty string");

  // one char
  searchBox.setAttribute("value", tabNames[0].charAt(0));
  matchResults = getMatchResults(contentWindow, searchBox.getAttribute("value"));
  ok(matchResults.length == 0,
     "Match nothing if the length of search term is less than 2");

  // the full title
  searchBox.setAttribute("value", tabNames[2]);
  matchResults = getMatchResults(contentWindow, searchBox.getAttribute("value"));
  is(matchResults.length, 1,
    "Match something when the whole title exists");

  // part of titled
  searchBox.setAttribute("value", tabNames[0].substr(1));
  contentWindow.Search.perform();
  matchResults = getMatchResults(contentWindow, searchBox.getAttribute("value"));
  is(matchResults.length, 1,
     "Match something when a part of title exists");

  cleanup(contentWindow);
}

function cleanup(contentWindow) {
  contentWindow.Search.hide(null);

  let onTabViewHidden = function() {
      window.removeEventListener("tabviewhidden", onTabViewHidden, false);
      ok(!TabView.isVisible(), "Tab View is hidden");
      let numToClose = newWindows.length;
      newWindows.forEach(function(win) {
        whenWindowObservesOnce(win, "domwindowclosed", function() {
          --numToClose;
          if(numToClose==0) {
            finish();
          }
        });
        win.close();
      });
  }
  window.addEventListener("tabviewhidden", onTabViewHidden, false);
  EventUtils.synthesizeKey("VK_RETURN", {});
}

function whenWindowObservesOnce(win, topic, func) {
    let windowWatcher = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
      .getService(Components.interfaces.nsIWindowWatcher);
    let origWin = win;
    let origTopic = topic;
    let origFunc = func;        
    function windowObserver(aSubject, aTopic, aData) {
      let theWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      if (origWin && theWin != origWin)
        return;
      if(aTopic == origTopic) {
          windowWatcher.unregisterNotification(windowObserver);
          origFunc.apply(this, []);
      }
    }
    windowWatcher.registerNotification(windowObserver);
}
