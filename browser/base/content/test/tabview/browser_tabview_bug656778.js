/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(TabView.PREF_FIRST_RUN);
    Services.prefs.clearUserPref(TabView.PREF_STARTUP_PAGE);
    Services.prefs.clearUserPref(TabView.PREF_RESTORE_ENABLED_ONCE);
  });

  let assertBoolPref = function (pref, value) {
    is(Services.prefs.getBoolPref(pref), value, pref + " is " + value);
  };

  let assertIntPref = function (pref, value) {
    is(Services.prefs.getIntPref(pref), value, pref + " is " + value);
  };

  let setPreferences = function (startupPage, firstRun, enabledOnce) {
    Services.prefs.setIntPref(TabView.PREF_STARTUP_PAGE, startupPage);
    Services.prefs.setBoolPref(TabView.PREF_FIRST_RUN, firstRun);
    Services.prefs.setBoolPref(TabView.PREF_RESTORE_ENABLED_ONCE, enabledOnce);
  };

  let assertPreferences = function (startupPage, firstRun, enabledOnce) {
    assertIntPref(TabView.PREF_STARTUP_PAGE, startupPage);
    assertBoolPref(TabView.PREF_FIRST_RUN, firstRun);
    assertBoolPref(TabView.PREF_RESTORE_ENABLED_ONCE, enabledOnce);
  };

  let assertNotificationBannerVisible = function (win) {
    let cw = win.TabView.getContentWindow();
    is(cw.iQ(".banner").length, 1, "notification banner is visible");
  };

  let assertNotificationBannerNotVisible = function (win) {
    let cw = win.TabView.getContentWindow();
    is(cw.iQ(".banner").length, 0, "notification banner is not visible");
  };

  let next = function () {
    if (tests.length == 0) {
      waitForFocus(finish);
      return;
    }

    let test = tests.shift();
    info("running " + test.name + "...");
    test();
  };

  // State:
  // Panorama was already used before (firstUseExperienced = true) but session
  // restore is deactivated. We did not automatically enable SR, yet.
  //
  // Expected result:
  // When entering Panorma session restore will be enabled and a notification
  // banner is shown.
  let test1 = function test1() {
    setPreferences(1, true, false);

    newWindowWithTabView(function (win) {
      assertNotificationBannerVisible(win);
      assertPreferences(3, true, true);

      win.close();
      next();
    });
  };

  // State:
  // Panorama has not been used before (firstUseExperienced = false) and session
  // restore is deactivated. We did not automatically enable SR, yet. That state
  // is equal to starting the browser the first time.
  //
  // Expected result:
  // When entering Panorma nothing happens. When we detect that Panorama is
  // really used (firstUseExperienced = true) we notify that session restore
  // is now enabled.
  let test2 = function test2() {
    setPreferences(1, false, false);

    newWindowWithTabView(function (win) {
      assertNotificationBannerNotVisible(win);
      assertPreferences(1, false, false);

      win.TabView.firstUseExperienced = true;

      assertNotificationBannerVisible(win);
      assertPreferences(3, true, true);

      win.close();
      next();
    });
  };

  // State:
  // Panorama was already used before (firstUseExperienced = true) and session
  // restore is activated. We did not automatically enable SR, yet.
  //
  // Expected result:
  // When entering Panorama nothing happens because session store is already
  // enabled so there's no reason to notify.
  let test3 = function test3() {
    setPreferences(3, true, false);

    newWindowWithTabView(function (win) {
      assertNotificationBannerNotVisible(win);
      assertPreferences(3, true, true);

      win.close();
      next();
    });
  };

  // State:
  // Panorama was already used before (firstUseExperienced = true) and session
  // restore has been automatically activated.
  //
  // Expected result:
  // When entering Panorama nothing happens.
  let test4 = function test4() {
    setPreferences(3, true, true);

    newWindowWithTabView(function (win) {
      assertNotificationBannerNotVisible(win);
      assertPreferences(3, true, true);

      win.close();
      next();
    });
  };

  // State:
  // Panorama was already used before (firstUseExperienced = true) and session
  // restore has been automatically activated. Session store was afterwards
  // disabled by the user so we won't touch that again.
  //
  // Expected result:
  // When entering Panorama nothing happens and we didn't enable session restore.
  let test5 = function test5() {
    setPreferences(1, true, true);

    newWindowWithTabView(function (win) {
      assertNotificationBannerNotVisible(win);
      assertPreferences(1, true, true);

      win.close();
      next();
    });
  };

  let tests = [test1, test2, test3, test4, test5];
  next();
}
