/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_REPLACED =
  "data:text/html;charset=utf8,<!DOCTYPE html><script>console = {log: () => ''}</script>";
const TEST_URI_NOT_REPLACED =
  "data:text/html;charset=utf8,<!DOCTYPE html><script>console.log('foo')</script>";

add_task(async function () {
  await pushPref("devtools.webconsole.timestampMessages", true);
  await pushPref("devtools.webconsole.persistlog", true);

  let hud = await openNewTabAndConsole(TEST_URI_NOT_REPLACED);

  await testWarningNotPresent(hud);
  await closeToolbox();

  // Use BrowserTestUtils instead of navigateTo as there is no toolbox opened
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    TEST_URI_REPLACED
  );
  await onBrowserLoaded;

  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "webconsole");
  hud = toolbox.getCurrentPanel().hud;
  await testWarningPresent(hud);
});

async function testWarningNotPresent(hud) {
  ok(!findWarningMessage(hud, "logging API"), "no warning displayed");

  // Bug 862024: make sure the warning doesn't show after page reload.
  info(
    "wait for the page to refresh and make sure the warning still isn't there"
  );
  await reloadBrowser();
  await waitFor(() => {
    return (
      findConsoleAPIMessages(hud, "foo").length === 2 &&
      findMessagesByType(hud, "foo", ".navigationMarker").length === 1
    );
  });

  ok(!findWarningMessage(hud, "logging API"), "no warning displayed");
}

async function testWarningPresent(hud) {
  info("wait for the warning to show");
  await waitFor(() => findWarningMessage(hud, "logging API"));

  info("reload the test page and wait for the warning to show");
  await reloadBrowser();
  await waitFor(() => {
    return findWarningMessages(hud, "logging API").length === 2;
  });
}
