/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

const {DevToolsShim} = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});
const {gDevTools} = DevToolsShim;

const DEVTOOLS_THEME_PREF = "devtools.theme";

/**
 * This test file ensures that:
 *
 * - devtools.panels.themeName returns the correct value,
 *   both from a page and a panel.
 * - devtools.panels.onThemeChanged fires for theme changes,
 *   both from a page and a panel.
 * - devtools.panels.create is able to create a devtools panel.
 */

async function switchTheme(theme) {
  const waitforThemeChanged = new Promise(resolve => gDevTools.once("theme-changed", resolve));
  Preferences.set(DEVTOOLS_THEME_PREF, theme);
  await waitforThemeChanged;
}

async function testThemeSwitching(extension, locations = ["page"]) {
  for (let newTheme of ["dark", "light"]) {
    await switchTheme(newTheme);
    for (let location of locations) {
      is(await extension.awaitMessage(`devtools_theme_changed_${location}`),
        newTheme,
        `The onThemeChanged event listener fired for the ${location}.`);
      is(await extension.awaitMessage(`current_theme_${location}`),
        newTheme,
        `The current theme is reported as expected for the ${location}.`);
    }
  }
}

add_task(async function test_theme_name_no_panel() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function devtools_page() {
    browser.devtools.panels.onThemeChanged.addListener(themeName => {
      browser.test.sendMessage("devtools_theme_changed_page", themeName);
      browser.test.sendMessage("current_theme_page", browser.devtools.panels.themeName);
    });

    browser.test.sendMessage("initial_theme", browser.devtools.panels.themeName);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  // Ensure that the initial value of the devtools theme is "light".
  await SpecialPowers.pushPrefEnv({set: [[DEVTOOLS_THEME_PREF, "light"]]});

  await extension.startup();

  let target = gDevTools.getTargetForTab(tab);
  await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  is(await extension.awaitMessage("initial_theme"),
    "light",
    "The initial theme is reported as expected.");

  await testThemeSwitching(extension);

  await gDevTools.closeToolbox(target);
  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_devtools_page_panels_create() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function devtools_page() {
    const result = {
      devtoolsPageTabId: browser.devtools.inspectedWindow.tabId,
      panelCreated: 0,
      panelShown: 0,
      panelHidden: 0,
    };

    try {
      const panel = await browser.devtools.panels.create(
        "Test Panel", "fake-icon.png", "devtools_panel.html"
      );

      result.panelCreated++;

      panel.onShown.addListener(contentWindow => {
        result.panelShown++;
        browser.test.assertEq("complete", contentWindow.document.readyState,
                              "Got the expected 'complete' panel document readyState");
        browser.test.assertEq("test_panel_global", contentWindow.TEST_PANEL_GLOBAL,
                              "Got the expected global in the panel contentWindow");
        browser.test.sendMessage("devtools_panel_shown", result);
      });

      panel.onHidden.addListener(() => {
        result.panelHidden++;

        browser.test.sendMessage("devtools_panel_hidden", result);
      });

      browser.devtools.panels.onThemeChanged.addListener(themeName => {
        browser.test.sendMessage("devtools_theme_changed_page", themeName);
        browser.test.sendMessage("current_theme_page", browser.devtools.panels.themeName);
      });

      browser.test.sendMessage("devtools_panel_created");
      browser.test.sendMessage("initial_theme_page", browser.devtools.panels.themeName);
    } catch (err) {
      // Make the test able to fail fast when it is going to be a failure.
      browser.test.sendMessage("devtools_panel_created");
      throw err;
    }
  }

  function devtools_panel() {
    // Set a property in the global and check that it is defined
    // and accessible from the devtools_page when the panel.onShown
    // event has been received.
    window.TEST_PANEL_GLOBAL = "test_panel_global";

    browser.devtools.panels.onThemeChanged.addListener(themeName => {
      browser.test.sendMessage("devtools_theme_changed_panel", themeName);
      browser.test.sendMessage("current_theme_panel", browser.devtools.panels.themeName);
    });

    browser.test.sendMessage("devtools_panel_inspectedWindow_tabId",
                             browser.devtools.inspectedWindow.tabId);
    browser.test.sendMessage("initial_theme_panel", browser.devtools.panels.themeName);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
      "devtools_panel.html":  `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         DEVTOOLS PANEL
         <script src="devtools_panel.js"></script>
       </body>
      </html>`,
      "devtools_panel.js": devtools_panel,
    },
  });

  registerCleanupFunction(function() {
    Preferences.reset(DEVTOOLS_THEME_PREF);
  });

  // Ensure that the initial value of the devtools theme is "light".
  Preferences.set(DEVTOOLS_THEME_PREF, "light");

  await extension.startup();

  let target = gDevTools.getTargetForTab(tab);

  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  await extension.awaitMessage("devtools_panel_created");
  is(await extension.awaitMessage("initial_theme_page"),
    "light",
    "The initial theme is reported as expected from a devtools page.");

  const toolboxAdditionalTools = toolbox.getAdditionalTools();

  is(toolboxAdditionalTools.length, 1,
     "Got the expected number of toolbox specific panel registered.");

  await testThemeSwitching(extension);

  const panelDef = toolboxAdditionalTools[0];
  const panelId = panelDef.id;

  is(panelDef.invertIconForLightTheme, false,
     "devtools.panel.create set invertIconForLightTheme to false by default");

  await gDevTools.showToolbox(target, panelId);
  const {devtoolsPageTabId} = await extension.awaitMessage("devtools_panel_shown");
  const devtoolsPanelTabId = await extension.awaitMessage("devtools_panel_inspectedWindow_tabId");
  is(devtoolsPanelTabId, devtoolsPageTabId,
     "Got the same devtools.inspectedWindow.tabId from devtools page and panel");
  is(await extension.awaitMessage("initial_theme_panel"),
    "light",
    "The initial theme is reported as expected from a devtools panel.");
  info("Addon Devtools Panel shown");

  await testThemeSwitching(extension, ["page", "panel"]);

  await gDevTools.showToolbox(target, "webconsole");
  const results = await extension.awaitMessage("devtools_panel_hidden");
  info("Addon Devtools Panel hidden");

  is(results.panelCreated, 1, "devtools.panel.create callback has been called once");
  is(results.panelShown, 1, "panel.onShown listener has been called once");
  is(results.panelHidden, 1, "panel.onHidden listener has been called once");

  await gDevTools.showToolbox(target, panelId);
  await extension.awaitMessage("devtools_panel_shown");
  info("Addon Devtools Panel shown - second cycle");

  await gDevTools.showToolbox(target, "webconsole");
  const secondCycleResults = await extension.awaitMessage("devtools_panel_hidden");
  info("Addon Devtools Panel hidden - second cycle");

  is(secondCycleResults.panelCreated, 1, "devtools.panel.create callback has been called once");
  is(secondCycleResults.panelShown, 2, "panel.onShown listener has been called twice");
  is(secondCycleResults.panelHidden, 2, "panel.onHidden listener has been called twice");

  // Turn off the addon devtools panel using the visibilityswitch.
  const waitToolVisibilityOff = new Promise(resolve => {
    toolbox.once("tool-unregistered", resolve);
  });

  Services.prefs.setBoolPref(`devtools.webext-${panelId}.enabled`, false);
  gDevTools.emit("tool-unregistered", panelId);

  await waitToolVisibilityOff;

  ok(toolbox.hasAdditionalTool(panelId),
     "The tool has not been removed on visibilityswitch set to false");

  is(toolbox.visibleAdditionalTools.filter(tool => tool.id == panelId).length, 0,
     "The tool is not visible on visibilityswitch set to false");

  // Turn on the addon devtools panel using the visibilityswitch.
  const waitToolVisibilityOn = new Promise(resolve => {
    toolbox.once("tool-registered", resolve);
  });

  Services.prefs.setBoolPref(`devtools.webext-${panelId}.enabled`, true);
  gDevTools.emit("tool-registered", panelId);

  await waitToolVisibilityOn;

  ok(toolbox.hasAdditionalTool(panelId),
     "The tool has been added on visibilityswitch set to true");
  is(toolbox.visibleAdditionalTools.filter(toolId => toolId == panelId).length, 1,
     "The tool is visible on visibilityswitch set to true");

  // Test devtools panel is loaded correctly after being toggled and
  // devtools panel events has been fired as expected.
  await gDevTools.showToolbox(target, panelId);
  await extension.awaitMessage("devtools_panel_shown");
  is(await extension.awaitMessage("initial_theme_panel"),
    "light",
    "The initial theme is reported as expected from a devtools panel.");
  info("Addon Devtools Panel shown - after visibilityswitch toggled");

  info("Wait until the Addon Devtools Panel has been loaded - after visibilityswitch toggled");
  const panelTabIdAfterToggle = await extension.awaitMessage("devtools_panel_inspectedWindow_tabId");
  is(panelTabIdAfterToggle, devtoolsPageTabId,
     "Got the same devtools.inspectedWindow.tabId from devtools panel after visibility toggled");

  await gDevTools.showToolbox(target, "webconsole");
  const toolToggledResults = await extension.awaitMessage("devtools_panel_hidden");
  info("Addon Devtools Panel hidden - after visibilityswitch toggled");

  is(toolToggledResults.panelCreated, 1, "devtools.panel.create callback has been called once");
  is(toolToggledResults.panelShown, 3, "panel.onShown listener has been called three times");
  is(toolToggledResults.panelHidden, 3, "panel.onHidden listener has been called three times");

  await gDevTools.closeToolbox(target);
  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
