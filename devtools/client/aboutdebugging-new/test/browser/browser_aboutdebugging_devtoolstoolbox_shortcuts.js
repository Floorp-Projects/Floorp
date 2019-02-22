/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

/**
 * Test shortcut keys on about:devtools-toolbox page.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();

  info("Show about:devtools-toolbox page");
  const target = findDebugTargetByText("about:debugging", document);
  ok(target, "about:debugging tab target appeared");
  const inspectButton = target.querySelector(".js-debug-target-inspect-button");
  ok(inspectButton, "Inspect button for about:debugging appeared");
  inspectButton.click();
  await Promise.all([
    waitUntil(() => tab.nextElementSibling),
    waitForRequestsToSettle(window.AboutDebugging.store),
    gDevTools.once("toolbox-ready"),
  ]);

  info("Wait for about:devtools-toolbox tab will be selected");
  const devtoolsTab = tab.nextElementSibling;
  await waitUntil(() => gBrowser.selectedTab === devtoolsTab);
  info("Check whether the shortcut keys which opens devtools is disabled");
  await assertShortcutKeys(gBrowser.selectedBrowser, false);

  info("Force to select about:debugging page");
  gBrowser.selectedTab = tab;
  info("Check whether the shortcut keys which opens devtools is enabled");
  await assertShortcutKeys(gBrowser.selectedBrowser, true);

  await closeAboutDevtoolsToolbox(devtoolsTab, window);
  await removeTab(tab);
});

async function assertShortcutKeys(browser, shouldBeEnabled) {
  await assertShortcutKey(browser.contentWindow, "VK_F12", {}, shouldBeEnabled);
  await assertShortcutKey(browser.contentWindow, "I", {
    accelKey: true,
    shiftKey: !navigator.userAgent.match(/Mac/),
    altKey: navigator.userAgent.match(/Mac/),
  }, shouldBeEnabled);
}

async function assertShortcutKey(win, key, modifiers, shouldBeEnabled) {
  info(`Assert shortcut key [${ key }]`);

  if (shouldBeEnabled) {
    await assertShortcutKeyEnabled(win, key, modifiers);
  } else {
    await assertShortcutKeyDisabled(win, key, modifiers);
  }
}

async function assertShortcutKeyDisabled(win, key, modifiers) {
  let isReadyCalled = false;
  const toolboxListener = () => {
    isReadyCalled = true;
  };
  gDevTools.on("toolbox-ready", toolboxListener);

  EventUtils.synthesizeKey(key, modifiers, win);
  await wait(1000);
  ok(!isReadyCalled, `Devtools should not be opened by ${ key }`);

  gDevTools.off("toolbox-ready", toolboxListener);
}

async function assertShortcutKeyEnabled(win, key, modifiers) {
  // Open devtools
  const onToolboxReady = gDevTools.once("toolbox-ready");
  EventUtils.synthesizeKey(key, modifiers, win);
  await onToolboxReady;
  ok(true, `Devtools should be opened by ${ key }`);

  // Close devtools
  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");
  EventUtils.synthesizeKey(key, modifiers, win);
  await onToolboxDestroyed;
  ok(true, `Devtopls should be closed by ${ key }`);
}
