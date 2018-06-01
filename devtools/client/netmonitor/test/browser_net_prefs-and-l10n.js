/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the preferences and localization objects work correctly.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { monitor } = await initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  const { windowRequire } = monitor.panelWin;
  const { Prefs } = windowRequire("devtools/client/netmonitor/src/utils/prefs");

  testL10N();
  testPrefs();

  return teardown(monitor);

  function testL10N() {
    is(typeof L10N.getStr("netmonitor.security.enabled"), "string",
      "The getStr() method didn't return a valid string.");
    is(typeof L10N.getFormatStr("networkMenu.totalMS", "foo"), "string",
      "The getFormatStr() method didn't return a valid string.");
  }

  function testPrefs() {
    is(Prefs.networkDetailsWidth,
      Services.prefs.getIntPref("devtools.netmonitor.panes-network-details-width"),
      "Getting a pref should work correctly.");

    const previousValue = Prefs.networkDetailsWidth;
    const bogusValue = ~~(Math.random() * 100);
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
