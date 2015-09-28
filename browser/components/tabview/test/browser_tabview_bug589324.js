/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const DUMMY_PAGE_URL = "http://mochi.test:8888/browser/browser/components/tabview/test/dummy_page.html";
const DUMMY_PAGE_URL_2 = "http://mochi.test:8888/";

var state = {
  windows: [{
    tabs: [{
      entries: [{ url: DUMMY_PAGE_URL }],
      hidden: true,
      attributes: {},
      extData: {
        "tabview-tab":
          '{"bounds":{"left":21,"top":29,"width":204,"height":153},' +
          '"userSize":null,"url":"' + DUMMY_PAGE_URL + '","groupID":1,' + 
          '"imageData":null,"title":null}'
      }
    },{
      entries: [{ url: DUMMY_PAGE_URL_2 }],
      hidden: false,
      attributes: {},
      extData: {
        "tabview-tab": 
          '{"bounds":{"left":315,"top":29,"width":111,"height":84},' + 
          '"userSize":null,"url":"' + DUMMY_PAGE_URL_2 + '","groupID":2,' + 
          '"imageData":null,"title":null}'
      },
    }],
    selected:2,
    _closedTabs: [],
    extData: {
      "tabview-groups": '{"nextID":3,"activeGroupId":2}',
      "tabview-group": 
        '{"1":{"bounds":{"left":15,"top":5,"width":280,"height":232},' + 
        '"userSize":null,"title":"","id":1},' + 
        '"2":{"bounds":{"left":309,"top":5,"width":267,"height":226},' + 
        '"userSize":null,"title":"","id":2}}',
      "tabview-ui": '{"pageBounds":{"left":0,"top":0,"width":788,"height":548}}'
    }, sizemode:"normal"
  }]
};

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("browser.sessionstore.restore_hidden_tabs");
  });

  Services.prefs.setBoolPref("browser.sessionstore.restore_hidden_tabs", false);

  testTabSwitchAfterRestore(function () {
    Services.prefs.setBoolPref("browser.sessionstore.restore_hidden_tabs", true);
    testTabSwitchAfterRestore(finish);
  });
}

function testTabSwitchAfterRestore(callback) {
  newWindowWithState(state, function (win) {
    registerCleanupFunction(() => win.close());

    let [firstTab, secondTab] = win.gBrowser.tabs;
    is(firstTab.linkedBrowser.currentURI.spec, DUMMY_PAGE_URL,
       "The url of first tab url is dummy_page.html");

    // check the hidden state of both tabs.
    ok(firstTab.hidden, "The first tab is hidden");
    ok(!secondTab.hidden, "The second tab is not hidden");
    is(secondTab, win.gBrowser.selectedTab, "The second tab is selected");

    // when the second tab is hidden, Panorama should be initialized and
    // the first tab should be visible.
    let container = win.gBrowser.tabContainer;
    container.addEventListener("TabHide", function onTabHide() {
      container.removeEventListener("TabHide", onTabHide, false);

      ok(win.TabView.getContentWindow(), "Panorama is loaded");
      ok(!firstTab.hidden, "The first tab is not hidden");
      is(firstTab, win.gBrowser.selectedTab, "The first tab is selected");
      ok(secondTab.hidden, "The second tab is hidden");

      callback();
    }, false);

    // switch to another tab
    win.switchToTabHavingURI(DUMMY_PAGE_URL);
  });
}
