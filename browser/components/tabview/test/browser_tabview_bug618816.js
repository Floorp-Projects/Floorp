/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let cw;
  let win;

  let testFocusTitle = function () {
    let title = 'title';
    let groupItem = cw.GroupItems.groupItems[0];
    groupItem.setTitle(title);

    let target = groupItem.$titleShield[0];
    EventUtils.synthesizeMouseAtCenter(target, {}, cw);

    let input = groupItem.$title[0];
    is(input.selectionStart, 0, 'the whole text is selected');
    is(input.selectionEnd, title.length, 'the whole text is selected');

    EventUtils.synthesizeMouseAtCenter(input, {}, cw);
    is(input.selectionStart, title.length, 'caret is at the rightmost position and no text is selected');
    is(input.selectionEnd, title.length, 'caret is at the rightmost position and no text is selected');

    win.close();
    finish();
  }

  waitForExplicitFinish();

  newWindowWithTabView(function (tvwin) {
    win = tvwin;

    registerCleanupFunction(function () {
      if (!win.closed)
        win.close();
    });

    cw = win.TabView.getContentWindow();
    SimpleTest.waitForFocus(testFocusTitle, cw);
  });
}
