/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let branch = Services.prefs.getBranch('browser.panorama.');
  branch.setBoolPref('experienced_first_run', false);
  
  newWindowWithTabView(function (win) {
    is(win.gBrowser.visibleTabs.length, 1, 'There should be one visible tab, only');

    win.TabView._initFrame(function () {
      is(win.gBrowser.visibleTabs.length, 1, 'There should be one visible tab, only');

      win.close();
      finish();
    });
  });
}
