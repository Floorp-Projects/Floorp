/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewWindowLoaded);
}

function onTabViewWindowLoaded(win) {
  win.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;

  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  // Create a group and make it active
  let box = new contentWindow.Rect(100, 100, 370, 370);
  let group = new contentWindow.GroupItem([], { bounds: box });
  ok(group.isEmpty(), "This group is empty");
  contentWindow.UI.setActive(group);
  is(contentWindow.GroupItems.getActiveGroupItem(), group, "new group is active");
  
  // Create a bunch of tabs in the group
  let bg = {inBackground: true};
  let datatext = win.gBrowser.loadOneTab("data:text/plain,bug610242", bg);
  let datahtml = win.gBrowser.loadOneTab("data:text/html,<blink>don't blink!</blink>", bg);
  let mozilla  = win.gBrowser.loadOneTab("about:mozilla", bg);
  let robots   = win.gBrowser.loadOneTab("about:robots", bg);
  let html     = win.gBrowser.loadOneTab("http://example.com", bg);
  let png      = win.gBrowser.loadOneTab("http://mochi.test:8888/browser/browser/base/content/test/moz.png", bg);
  let svg      = win.gBrowser.loadOneTab("http://mochi.test:8888/browser/browser/base/content/test/title_test.svg", bg);
  
  ok(!group.shouldStack(group._children.length), "Group should not stack.");
  
  // PREPARE FINISH:
  group.addSubscriber("close", function onClose() {
    group.removeSubscriber("close", onClose);

    ok(group.isEmpty(), "The group is empty again");

    contentWindow.UI.setActive(currentGroup);
    isnot(contentWindow.GroupItems.getActiveGroupItem(), null, "There is an active group");
    is(win.gBrowser.tabs.length, 1, "There is only one tab left");
    is(win.gBrowser.visibleTabs.length, 1, "There is also only one visible tab");

    whenTabViewIsHidden(function() {
      win.close();
      ok(win.closed, "new window is closed");
      finish();
    }, win);
    win.gBrowser.selectedTab = originalTab;

    win.TabView.hide();
  });

  function check(tab, label, visible) {
    let display = contentWindow.getComputedStyle(tab._tabViewTabItem.$fav[0], null).getPropertyValue("display");
    if (visible) {
      is(display, "block", label + " has favicon");
    } else {
      is(display, "none", label + " has no favicon");
    }
  }

  afterAllTabsLoaded(function() {
    let children = group.getChildren();
    let len = children.length;
    let iconUpdateCounter = 0;

    children.forEach(function(tabItem) {
      tabItem.addSubscriber("iconUpdated", function onIconUpdated() {
        tabItem.removeSubscriber("iconUpdated", onIconUpdated);

        if (++iconUpdateCounter == len) {
          check(datatext, "datatext", false);
          check(datahtml, "datahtml", false);
          check(mozilla, "about:mozilla", false);
          check(robots, "about:robots", true);
          check(html, "html", true);
          check(png, "png", false);
          check(svg, "svg", true);

          // Get rid of the group and its children
          // The group close will trigger a finish().
          closeGroupItem(group);
        }
      });
    });

    afterAllTabItemsUpdated(function () {}, win);
  }, win);
}
