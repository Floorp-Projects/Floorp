/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const fi = Cc["@mozilla.org/browser/favicon-service;1"].
           getService(Ci.nsIFaviconService);

let newTab;

function test() {
  waitForExplicitFinish();

  newTab = gBrowser.addTab();

  showTabView(function() {
    whenAppTabIconAdded(onTabPinned);
    gBrowser.pinTab(newTab);
  })
}

function onTabPinned() {
  let contentWindow = document.getElementById("tab-view").contentWindow;
  is(contentWindow.GroupItems.groupItems.length, 1, 
     "There is one group item on startup");

  let groupItem = contentWindow.GroupItems.groupItems[0];
  let icon = contentWindow.iQ(".appTabIcon", groupItem.$appTabTray)[0];
  let $icon = contentWindow.iQ(icon);

  is($icon.data("xulTab"), newTab, 
     "The app tab icon has the right tab reference")
  // check to see whether it's showing the default one or not.
  is($icon.attr("src"), fi.defaultFavicon.spec,
     "The icon is showing the default fav icon for blank tab");

  let errorHandler = function(event) {
    newTab.removeEventListener("error", errorHandler, false);

    // since the browser code and test code are invoked when an error event is 
    // fired, a delay is used here to avoid the test code run before the browser 
    // code.
    executeSoon(function() {
      let iconSrc = $icon.attr("src");
      let hasData = true;
      try {
        fi.getFaviconDataAsDataURL(iconSrc);
      } catch(e) {
        hasData = false;
      }
      ok(!hasData, "The icon src doesn't return any data");
      // with moz-anno:favicon automatically redirects to the default favIcon 
      // if the given url is invalid
      ok(/^moz-anno:favicon:/.test(iconSrc),
         "The icon url starts with moz-anno:favicon so the default fav icon would be displayed");

      // clean up
      gBrowser.removeTab(newTab);
      let endGame = function() {
        window.removeEventListener("tabviewhidden", endGame, false);

        ok(!TabView.isVisible(), "Tab View is hidden");
        finish();
      }
      window.addEventListener("tabviewhidden", endGame, false);
      TabView.toggle();
    });
  };
  newTab.addEventListener("error", errorHandler, false);

  newTab.linkedBrowser.loadURI(
    "http://mochi.test:8888/browser/browser/components/tabview/test/test_bug600645.html");
}
