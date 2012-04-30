/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let stateStartup = {windows:[
  {tabs:[{entries:[{url:"about:home"}]}], extData:{"tabview-last-session-group-name":"title"}}
]};

function test() {
  let assertWindowTitle = function (win, title) {
    let browser = win.gBrowser.tabs[0].linkedBrowser;
    let winTitle = win.gBrowser.getWindowTitleForBrowser(browser);

    info('window title is: "' + winTitle + '"');
    is(winTitle.indexOf(title), 0, "title starts with '" + title + "'");
  };

  let testGroupNameChange = function (win) {
    showTabView(function () {
      let cw = win.TabView.getContentWindow();
      let groupItem = cw.GroupItems.groupItems[0];
      groupItem.setTitle("new-title");

      hideTabView(function () {
        assertWindowTitle(win, "new-title");
        finish();
      }, win);
    }, win);
  };

  waitForExplicitFinish();

  newWindowWithState(stateStartup, function (win) {
    registerCleanupFunction(function () win.close());
    assertWindowTitle(win, "title");
    testGroupNameChange(win);
  });
}
