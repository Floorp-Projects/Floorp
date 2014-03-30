/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Raymond Lee <raymond@appcoast.com>
 *   Ian Gilman <ian@iangilman.com>
 */

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewShown);
}

function onTabViewShown(win) {
  let TabView = win.TabView;
  let gBrowser = win.gBrowser;
  let document = win.document;

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let iQ = contentWindow.iQ;

  // establish initial state
  is(contentWindow.GroupItems.groupItems.length, 1,
      "we start with one group (the default)");
  is(gBrowser.tabs.length, 1, "we start with one tab");
  let originalTab = gBrowser.tabs[0];

  // create a group
  let box = new contentWindow.Rect(20, 20, 210, 200);
  let groupItem = new contentWindow.GroupItem([],
      { bounds: box, title: "test1" });
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  contentWindow.UI.setActive(groupItem);

  // create a tab
  let xulTabs = [];
  xulTabs.push(gBrowser.loadOneTab("about:blank"));
  is(gBrowser.tabs.length, 2, "we now have two tabs");
  is(groupItem._children.length, 1, "the new tab was added to the group");

  // make sure the group has no app tabs
  is(appTabCount(groupItem), 0, "there are no app tab icons");

  let tray = groupItem.$appTabTray;
  let trayContainer = iQ(tray[0].parentNode);

  is(parseInt(trayContainer.css("width")), 0,
     "$appTabTray container is not visible");

  // pin the tab, make sure the TabItem goes away and the icon comes on
  whenAppTabIconAdded(groupItem, function () {
    is(groupItem._children.length, 0,
       "the app tab's TabItem was removed from the group");
    is(appTabCount(groupItem), 1, "there's now one app tab icon");

    is(tray.css("-moz-column-count"), 1,
       "$appTabTray column count is 1");
    isnot(parseInt(trayContainer.css("width")), 0,
       "$appTabTray container is visible");


    let iconHeight = iQ(iQ(".appTabIcon", tray)[0]).height();
    let trayHeight = parseInt(trayContainer.css("height"));
    let rows = Math.floor(trayHeight / iconHeight);
    let icons = rows * 2;

    function pinnedSomeTabs() {
      is(appTabCount(groupItem), icons, "number of app tab icons is correct");

      is(tray.css("-moz-column-count"), 2,
         "$appTabTray column count is 2");

      ok(!trayContainer.hasClass("appTabTrayContainerTruncated"),
         "$appTabTray container does not have .appTabTrayContainerTruncated");

      // add one more tab
      xulTabs.push(gBrowser.loadOneTab("about:blank"));
      whenAppTabIconAdded(groupItem, function () {
        is(tray.css("-moz-column-count"), 3,
           "$appTabTray column count is 3");

        ok(trayContainer.hasClass("appTabTrayContainerTruncated"),
           "$appTabTray container hasClass .appTabTrayContainerTruncated");

        // remove all but one app tabs
        for (let i = 1; i < xulTabs.length; i++)
          gBrowser.removeTab(xulTabs[i]);

        is(tray.css("-moz-column-count"), 1,
           "$appTabTray column count is 1");

        is(appTabCount(groupItem), 1, "there's now one app tab icon");

        ok(!trayContainer.hasClass("appTabTrayContainerTruncated"),
           "$appTabTray container does not have .appTabTrayContainerTruncated");

        // unpin the last remaining tab
        gBrowser.unpinTab(xulTabs[0]);

        is(parseInt(trayContainer.css("width")), 0,
           "$appTabTray container is not visible");

        // When the tab was pinned, the last active group with an item got the focus.
        // Therefore, switching the focus back to group item one.
        contentWindow.UI.setActive(groupItem);

        is(appTabCount(groupItem), 0, "there are no app tab icons");

        is(groupItem._children.length, 1, "the normal tab shows in the group");

        gBrowser.removeTab(xulTabs[0]);

        // close the group
        groupItem.close();

        hideTabView(function() {
          ok(!TabView.isVisible(), "Tab View is hidden");

          is(contentWindow.GroupItems.groupItems.length, 1,
             "we finish with one group");
          is(gBrowser.tabs.length, 1, "we finish with one tab");

          win.close();

          executeSoon(finish);
        }, win);
      });
      win.gBrowser.pinTab(xulTabs[xulTabs.length-1]);
    };

    // add enough tabs to have two columns
    let returnCount = 0;
    for (let i = 1; i < icons; i++) {
      xulTabs.push(gBrowser.loadOneTab("about:blank"));
      whenAppTabIconAdded(groupItem, function () {
        if (++returnCount == (icons - 1))
          executeSoon(pinnedSomeTabs);
      });
      win.gBrowser.pinTab(xulTabs[i]);
    }
  });
  win.gBrowser.pinTab(xulTabs[0]);
}

function appTabCount(groupItem) {
  return groupItem.container.getElementsByClassName("appTabIcon").length;
}

