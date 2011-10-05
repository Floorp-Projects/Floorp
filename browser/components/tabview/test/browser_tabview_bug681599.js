/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  showTabView(function() {
    let cw = TabView.getContentWindow();

    registerCleanupFunction(function () {
      while (gBrowser.tabs.length > 1)
        gBrowser.removeTab(gBrowser.tabs[1]);
      hideTabView();
    })

    whenSearchIsEnabled(function() {
      ok(cw.Search.isEnabled(), "The search is enabled before creating a new tab");

      whenTabViewIsHidden(function() {
        showTabView(function() {
          ok(!cw.Search.isEnabled(), "The search is disabled when entering Tabview");

          hideTabView(finish);
        })
      });
      EventUtils.synthesizeKey("t", { accelKey: true }, cw);
    });

    EventUtils.synthesizeKey("VK_SLASH", {}, cw);
  });
}
