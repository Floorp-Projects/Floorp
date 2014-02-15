/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function (win) {
    let cw = win.TabView.getContentWindow();

    registerCleanupFunction(function () {
      cw.gPrefBranch.clearUserPref("animate_zoom");
      win.close();
    });

    cw.gPrefBranch.setBoolPref("animate_zoom", false);

    let groupItem = cw.GroupItems.groupItems[0];
    let shield = groupItem.$titleShield[0];
    let keys = ["RETURN", "ESCAPE"];

    ok(win.TabView.isVisible(), "tabview is visible");

    let simulateKeyPress = function () {
      let key = keys.shift();

      if (!key) {
        ok(win.TabView.isVisible(), "tabview is still visible");
        finish();
        return;
      }

      EventUtils.synthesizeMouseAtCenter(shield, {}, cw);
      EventUtils.synthesizeKey("VK_" + key, {}, cw);

      executeSoon(function () {
        ok(win.TabView.isVisible(), "tabview is still visible [" + key + "]");
        simulateKeyPress();
      });
    }

    SimpleTest.waitForFocus(simulateKeyPress, cw);
  });
}
