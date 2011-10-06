/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 150, 150);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    cw.UI.setActive(groupItem);
    gBrowser.loadOneTab('about:blank', {inBackground: true});

    return groupItem;
  }

  let synthesizeMiddleMouseDrag = function (tabContainer, width) {
    EventUtils.synthesizeMouseAtCenter(tabContainer,
        {type: 'mousedown', button: 1}, cw);
    let rect = tabContainer.getBoundingClientRect();
    EventUtils.synthesizeMouse(tabContainer, rect.width / 2 + width,
        rect.height / 2, {type: 'mousemove', button: 1}, cw);
    EventUtils.synthesizeMouse(tabContainer, rect.width / 2 + width,
        rect.height / 2, {type: 'mouseup', button: 1}, cw);
  }

  let testDragAndDropWithMiddleMouseButton = function () {
    let groupItem = createGroupItem();
    let tabItem = groupItem.getChild(0);
    let tabContainer = tabItem.container;
    let bounds = tabItem.getBounds();

    // try to drag and move the mouse out of the tab
    synthesizeMiddleMouseDrag(tabContainer, 200);
    is(groupItem.getChild(0), tabItem, 'tabItem was not closed');
    ok(bounds.equals(tabItem.getBounds()), 'bounds did not change');

    // try to drag and let the mouse stay within tab bounds
    synthesizeMiddleMouseDrag(tabContainer, 10);
    ok(!groupItem.getChild(0), 'tabItem was closed');

    hideTabView(finish);
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    afterAllTabsLoaded(testDragAndDropWithMiddleMouseButton);
  });
}
