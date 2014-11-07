/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(function () win.close());

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];
    groupItem.setBounds(new cw.Rect(cw.innerWidth - 200, 0, 200, 200));

    whenTabViewIsHidden(function () waitForFocus(finish), win);

    waitForFocus(function () {
      let button = cw.document.getElementById("exit-button");
      EventUtils.synthesizeMouseAtCenter(button, {}, cw);
    }, cw);
  });
}
