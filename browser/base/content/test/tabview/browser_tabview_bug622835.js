/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(onTabViewShown);
}

function onTabViewShown(win) {
  let contentWindow = win.TabView.getContentWindow();

  let finishTest = function () {
    hideTabView(function () {
      win.close();
      finish();
    }, win);
  }

  // do not let the group arrange itself
  contentWindow.GroupItems.pauseArrange();

  // let's create a groupItem small enough to get stacked
  let groupItem = new contentWindow.GroupItem([], {
    immediately: true,
    bounds: {left: 20, top: 20, width: 100, height: 100}
  });

  contentWindow.UI.setActive(groupItem);

  // we need seven tabs at least to reproduce this
  for (var i=0; i<7; i++)
    win.gBrowser.loadOneTab('about:blank', {inBackground: true});

  // finally let group arrange
  contentWindow.GroupItems.resumeArrange();

  let tabItem6 = groupItem.getChildren()[5];
  let tabItem7 = groupItem.getChildren()[6];  
  ok(!tabItem6.getHidden(), 'the 6th child must not be hidden');
  ok(tabItem7.getHidden(), 'the 7th child must be hidden');

  finishTest();
}
