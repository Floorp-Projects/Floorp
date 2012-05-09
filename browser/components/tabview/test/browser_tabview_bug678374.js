/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ICON_URL = "moz-anno:favicon:http://example.com/browser/browser/components/tabview/test/test_bug678374_icon16.png";
const TEST_URL = "http://example.com/browser/browser/components/tabview/test/test_bug678374.html";

function test() {
  Services.prefs.setBoolPref("browser.chrome.favicons", false);

  waitForExplicitFinish();

  newWindowWithTabView(function(win) {
    is(win.gBrowser.tabs.length, 3, "There are 3 tabs")

    let newTabOne = win.gBrowser.tabs[1];
    let newTabTwo = win.gBrowser.tabs[2];
    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];

    // test tab item
    let newTabItemOne = newTabOne._tabViewTabItem;

    newTabItemOne.addSubscriber("iconUpdated", function onIconUpdated() {
      newTabItemOne.removeSubscriber("iconUpdated", onIconUpdated);
      is(newTabItemOne.$favImage[0].src, ICON_URL, "The tab item is showing the right icon.");

      // test pin tab
      whenAppTabIconAdded(groupItem, function() {
        let icon = cw.iQ(".appTabIcon", groupItem.$appTabTray)[0];
        is(icon.src, ICON_URL, "The app tab is showing the right icon");

        finish();
      });
      win.gBrowser.pinTab(newTabTwo);
    });
  }, function(win) {
    registerCleanupFunction(function() { 
      Services.prefs.clearUserPref("browser.chrome.favicons");
      win.close(); 
   });

    win.gBrowser.loadOneTab(TEST_URL);
    win.gBrowser.loadOneTab(TEST_URL);
  });
}
