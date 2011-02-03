/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;

  let createGroupItem = function () {
    let bounds = new cw.Rect(20, 20, 400, 200);
    let groupItem = new cw.GroupItem([], {bounds: bounds, immediately: true});

    let groupItemId = groupItem.id;
    registerCleanupFunction(function() {
      let groupItem = cw.GroupItems.groupItem(groupItemId);
      if (groupItem)
        groupItem.close();
    });

    return groupItem;
  }

  let testFocusTitle = function () {
    let title = 'title';
    let groupItem = createGroupItem();
    groupItem.setTitle(title);

    let target = groupItem.$titleShield[0];
    EventUtils.synthesizeMouseAtCenter(target, {}, cw);

    let input = groupItem.$title[0];
    is(input.selectionStart, 0, 'the whole text is selected');
    is(input.selectionEnd, title.length, 'the whole text is selected');

    EventUtils.synthesizeMouseAtCenter(input, {}, cw);
    is(input.selectionStart, title.length, 'caret is at the rightmost position and no text is selected');
    is(input.selectionEnd, title.length, 'caret is at the rightmost position and no text is selected');

    groupItem.close();
    hideTabView(finish);
  }

  waitForExplicitFinish();

  showTabView(function () {
    cw = TabView.getContentWindow();
    testFocusTitle();
  });
}
