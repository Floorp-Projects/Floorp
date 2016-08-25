/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the preferences and localization objects work correctly.
 */

add_task(function* () {
  let [,, monitor] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  ok(monitor.panelWin.L10N,
    "Should have a localization object available on the panel window.");
  ok(monitor.panelWin.Prefs,
    "Should have a preferences object available on the panel window.");

  testL10N();
  testPrefs();

  return teardown(monitor);

  function testL10N() {
    let { L10N } = monitor.panelWin;
    is(typeof L10N.getStr("netmonitor.label"), "string",
      "The getStr() method didn't return a valid string.");
    is(typeof L10N.getFormatStr("networkMenu.totalMS", "foo"), "string",
      "The getFormatStr() method didn't return a valid string.");
  }

  function testPrefs() {
    let { Prefs } = monitor.panelWin;

    is(Prefs.networkDetailsWidth,
      Services.prefs.getIntPref("devtools.netmonitor.panes-network-details-width"),
      "Getting a pref should work correctly.");

    let previousValue = Prefs.networkDetailsWidth;
    let bogusValue = ~~(Math.random() * 100);
    Prefs.networkDetailsWidth = bogusValue;
    is(Prefs.networkDetailsWidth,
      Services.prefs.getIntPref("devtools.netmonitor.panes-network-details-width"),
      "Getting a pref after it has been modified should work correctly.");
    is(Prefs.networkDetailsWidth, bogusValue,
      "The pref wasn't updated correctly in the preferences object.");

    Prefs.networkDetailsWidth = previousValue;
    is(Prefs.networkDetailsWidth,
      Services.prefs.getIntPref("devtools.netmonitor.panes-network-details-width"),
      "Getting a pref after it has been modified again should work correctly.");
    is(Prefs.networkDetailsWidth, previousValue,
      "The pref wasn't updated correctly again in the preferences object.");
  }
});
