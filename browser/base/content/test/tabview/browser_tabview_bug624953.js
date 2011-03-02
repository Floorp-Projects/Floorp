/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let finishTest = function (groupItem) {
    groupItem.addSubscriber(groupItem, 'groupHidden', function () {
      groupItem.removeSubscriber(groupItem, 'groupHidden');
      groupItem.closeHidden();
      hideTabView(finish);
    });

    groupItem.closeAll();
  }

  showTabView(function () {
    let cw = TabView.getContentWindow();

    let bounds = new cw.Rect(20, 20, 150, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});
    cw.GroupItems.setActiveGroupItem(groupItem);

    for (let i=0; i<4; i++)
      gBrowser.loadOneTab('about:blank', {inBackground: true});

    ok(!groupItem._isStacked, 'groupItem is not stacked');
    cw.GroupItems.pauseArrange();

    groupItem.setSize(150, 150);
    groupItem.setUserSize();
    ok(!groupItem._isStacked, 'groupItem is still not stacked');

    cw.GroupItems.resumeArrange();
    ok(groupItem._isStacked, 'groupItem is now stacked');

    finishTest(groupItem);
  });
}
