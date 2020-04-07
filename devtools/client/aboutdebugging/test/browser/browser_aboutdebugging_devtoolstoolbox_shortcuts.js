/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

/**
 * Test shortcut keys on about:devtools-toolbox page.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const {
    devtoolsBrowser,
    devtoolsTab,
    devtoolsWindow,
  } = await openAboutDevtoolsToolbox(document, tab, window);

  info("Check whether the shortcut keys which opens devtools is disabled");
  await assertShortcutKeys(devtoolsBrowser, false);

  info("Switch to the inspector programmatically");
  const toolbox = getToolbox(devtoolsWindow);
  await toolbox.selectTool("inspector");

  info(
    "Use the Webconsole keyboard shortcut and wait for the panel to be selected"
  );
  const onToolReady = toolbox.once("webconsole-ready");
  EventUtils.synthesizeKey(
    "K",
    {
      accelKey: true,
      shiftKey: !navigator.userAgent.match(/Mac/),
      altKey: navigator.userAgent.match(/Mac/),
    },
    devtoolsWindow
  );
  await onToolReady;

  info("Force to select about:debugging page");
  await updateSelectedTab(gBrowser, tab, window.AboutDebugging.store);

  info("Check whether the shortcut keys which opens devtools is enabled");
  await assertShortcutKeys(tab.linkedBrowser, true);

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});

async function assertShortcutKeys(browser, shouldBeEnabled) {
  await assertShortcutKey(browser.contentWindow, "VK_F12", {}, shouldBeEnabled);
  await assertShortcutKey(
    browser.contentWindow,
    "I",
    {
      accelKey: true,
      shiftKey: !navigator.userAgent.match(/Mac/),
      altKey: navigator.userAgent.match(/Mac/),
    },
    shouldBeEnabled
  );
}

async function assertShortcutKey(win, key, modifiers, shouldBeEnabled) {
  info(`Assert shortcut key [${key}]`);

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
  ok(!isReadyCalled, `Devtools should not be opened by ${key}`);

  gDevTools.off("toolbox-ready", toolboxListener);
}

async function assertShortcutKeyEnabled(win, key, modifiers) {
  // Open devtools
  const onToolboxReady = gDevTools.once("toolbox-ready");
  EventUtils.synthesizeKey(key, modifiers, win);
  await onToolboxReady;
  ok(true, `Devtools should be opened by ${key}`);

  // Close devtools
  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");
  EventUtils.synthesizeKey(key, modifiers, win);
  await onToolboxDestroyed;
  ok(true, `Devtopls should be closed by ${key}`);
}
