/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test verifies that the tab crash telemetry scalar is incremented
 * when a foreground tab crashes.
 */

const TABUI_PRESENTED_KEY = "dom.contentprocess.crash_tab_ui_presented";

add_task(async function test_crash_telemetry() {
  Services.telemetry.clearScalars();

  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await BrowserTestUtils.crashFrame(tab3.linkedBrowser, true, true);
  await BrowserTestUtils.crashFrame(tab1.linkedBrowser, false, true);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    TABUI_PRESENTED_KEY,
    1,
    "Tab crashed ui count."
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});
