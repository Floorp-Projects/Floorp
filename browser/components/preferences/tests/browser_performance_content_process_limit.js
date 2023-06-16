/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.defaultPerformanceSettings.enabled", false]],
  });

  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  is(prefs.selectedPane, "paneGeneral", "General pane was selected");

  let doc = gBrowser.contentDocument;

  let limitContentProcess = doc.querySelector("#limitContentProcess");
  is(
    limitContentProcess.hidden,
    Services.appinfo.fissionAutostart,
    "Limit Content Process should be hidden if fission is enabled and shown if it is not."
  );

  let contentProcessCount = doc.querySelector("#contentProcessCount");
  is(
    contentProcessCount.hidden,
    Services.appinfo.fissionAutostart,
    "Limit Content Count should be hidden if fission is enabled and shown if it is not."
  );

  let contentProcessCountEnabledDescription = doc.querySelector(
    "#contentProcessCountEnabledDescription"
  );
  is(
    contentProcessCountEnabledDescription.hidden,
    Services.appinfo.fissionAutostart,
    "Limit Content Process Enabled Description should be hidden if fission is enabled and shown if it is not."
  );

  let contentProcessCountDisabledDescription = doc.querySelector(
    "#contentProcessCountDisabledDescription"
  );
  is(
    contentProcessCountDisabledDescription.hidden,
    Services.appinfo.fissionAutostart ||
      Services.appinfo.browserTabsRemoteAutostart,
    "Limit Content Process Disabled Description should be shown if e10s is disabled, and hidden otherwise."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
