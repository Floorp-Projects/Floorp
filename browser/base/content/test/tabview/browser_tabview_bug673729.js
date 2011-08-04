/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    let cw = win.TabView.getContentWindow();

    // turn off zoom animations
    cw.gPrefBranch.setBoolPref("animate_zoom", false);

    registerCleanupFunction(function () {
      cw.gPrefBranch.clearUserPref("animate_zoom");
      win.close();
    });

    let group = cw.GroupItems.groupItems[0];
    group.setSize(100, 100, true);

    while (!group.isStacked())
      win.gBrowser.addTab();

    waitForFocus(function () {
      whenGroupIsExpanded(group, function () {
        ok(win.TabView.isVisible(), "tabview is visible");
        finish();
      });

      let expander = group.$expander[0];
      EventUtils.synthesizeMouseAtCenter(expander, {}, cw);
    }, cw);
  });
}

// ----------
function whenGroupIsExpanded(group, callback) {
  group.addSubscriber("expanded", function onExpanded() {
    group.removeSubscriber("expanded", onExpanded);
    executeSoon(callback);
  });
}
