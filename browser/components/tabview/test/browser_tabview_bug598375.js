/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    registerCleanupFunction(() => win.close());

    let cw = win.TabView.getContentWindow();
    let groupItem = cw.GroupItems.groupItems[0];
    groupItem.setBounds(new cw.Rect(cw.innerWidth - 200, 0, 200, 200));

    whenTabViewIsHidden(() => waitForFocus(finish), win);

    waitForFocus(function () {
      let button = cw.document.getElementById("exit-button");
      EventUtils.synthesizeMouseAtCenter(button, {}, cw);
    }, cw);
  });

  // This test relies on the test timing out in order to indicate failure so
  // let's add a dummy pass.
  ok(true, "Each test requires at least one pass, fail or todo so here is a pass.");
}
