/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {gDevTools} = require("devtools/client/framework/devtools");

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

async function openToolboxForTab(tab) {
  const target = gDevTools.getTargetForTab(tab);
  const toolbox = await gDevTools.showToolbox(target, "testBlankPanel");
  info("Developer toolbox opened");
  return {toolbox, target};
}

async function closeToolboxForTab(tab) {
  const target = gDevTools.getTargetForTab(tab);
  await gDevTools.closeToolbox(target);
  await target.destroy();
  info("Developer toolbox closed");
}

function createPage(jsScript, bodyText = "") {
  return `<!DOCTYPE html>
    <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         ${bodyText}
         <script src="${jsScript}"></script>
       </body>
    </html>`;
}

add_task(async function setup_blank_panel() {
  // Create a blank custom tool so that we don't need to wait the webconsole
  // to be fully loaded/unloaded to prevent intermittent failures (related
  // to a webconsole that is still loading when the test has been completed).
  const testBlankPanel = {
    id: "testBlankPanel",
    url: "about:blank",
    label: "Blank Tool",
    isTargetSupported() {
      return true;
    },
    build(iframeWindow, toolbox) {
      return Promise.resolve({
        target: toolbox.target,
        toolbox: toolbox,
        isReady: true,
        panelDoc: iframeWindow.document,
        destroy() {},
      });
    },
  };

  registerCleanupFunction(() => {
    gDevTools.unregisterTool(testBlankPanel.id);
  });

  gDevTools.registerTool(testBlankPanel);
});

async function test_theme_name(testWithPanel = false) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function switchTheme(theme) {
    const waitforThemeChanged = gDevTools.once("theme-changed");
    Preferences.set(DEVTOOLS_THEME_PREF, theme);
    return waitforThemeChanged;
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

  async function devtools_page(createPanel) {
    if (createPanel) {
      browser.devtools.panels.create(
        "Test Panel Theme", "fake-icon.png", "devtools_panel.html"
      );
    }

    browser.devtools.panels.onThemeChanged.addListener(themeName => {
      browser.test.sendMessage("devtools_theme_changed_page", themeName);
      browser.test.sendMessage("current_theme_page", browser.devtools.panels.themeName);
    });

    browser.test.sendMessage("initial_theme_page", browser.devtools.panels.themeName);
  }

  async function devtools_panel() {
    browser.devtools.panels.onThemeChanged.addListener(themeName => {
      browser.test.sendMessage("devtools_theme_changed_panel", themeName);
      browser.test.sendMessage("current_theme_panel", browser.devtools.panels.themeName);
    });

    browser.test.sendMessage("initial_theme_panel", browser.devtools.panels.themeName);
  }

  let files = {
    "devtools_page.html": createPage("devtools_page.js"),
    "devtools_page.js": `(${devtools_page})(${testWithPanel})`,
  };

  if (testWithPanel) {
    files["devtools_panel.js"] = devtools_panel;
    files["devtools_panel.html"] = createPage("devtools_panel.js", "Test Panel Theme");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files,
  });

  // Ensure that the initial value of the devtools theme is "light".
  await SpecialPowers.pushPrefEnv({set: [[DEVTOOLS_THEME_PREF, "light"]]});
  registerCleanupFunction(async function() {
    await SpecialPowers.popPrefEnv();
  });

  await extension.startup();

  const {toolbox, target} = await openToolboxForTab(tab);

  info("Waiting initial theme from devtools_page");
  is(await extension.awaitMessage("initial_theme_page"),
     "light",
     "The initial theme is reported as expected.");

  if (testWithPanel) {
    let toolboxAdditionalTools = toolbox.getAdditionalTools();
    is(toolboxAdditionalTools.length, 1,
       "Got the expected number of toolbox specific panel registered.");

    let panelId = toolboxAdditionalTools[0].id;

    await gDevTools.showToolbox(target, panelId);
    is(await extension.awaitMessage("initial_theme_panel"),
       "light",
       "The initial theme is reported as expected from a devtools panel.");

    await testThemeSwitching(extension, ["page", "panel"]);
  } else {
    await testThemeSwitching(extension);
  }

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_devtools_page_theme() {
  await test_theme_name(false);
});

add_task(async function test_devtools_panel_theme() {
  await test_theme_name(true);
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
        "Test Panel Create", "fake-icon.png", "devtools_panel.html"
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

      browser.test.sendMessage("devtools_panel_created");
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

    browser.test.sendMessage("devtools_panel_inspectedWindow_tabId",
                             browser.devtools.inspectedWindow.tabId);
  }

  const longPrefix = (new Array(80)).fill("x").join("");
  const EXTENSION_ID = `${longPrefix}@create-devtools-panel.test`;

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      devtools_page: "devtools_page.html",
      applications: {
        gecko: {id: EXTENSION_ID},
      },
    },
    files: {
      "devtools_page.html": createPage("devtools_page.js"),
      "devtools_page.js": devtools_page,
      "devtools_panel.html": createPage("devtools_panel.js", "Test Panel Create"),
      "devtools_panel.js": devtools_panel,
    },
  });

  await extension.startup();

  const extensionPrefBranch = `devtools.webextensions.${EXTENSION_ID}.`;
  const extensionPrefName = `${extensionPrefBranch}enabled`;

  let prefBranch = Services.prefs.getBranch(extensionPrefBranch);
  ok(prefBranch, "The preference branch for the extension should have been created");
  is(prefBranch.getBoolPref("enabled", false), true,
     "The 'enabled' bool preference for the extension should be initially true");


  // Get the devtools panel id from the first item in the toolbox additional tools array.
  const getPanelId = (toolbox) => {
    let toolboxAdditionalTools = toolbox.getAdditionalTools();
    is(toolboxAdditionalTools.length, 1,
       "Got the expected number of toolbox specific panel registered.");
    return toolboxAdditionalTools[0].id;
  };

  // Test the devtools panel shown and hide events.
  const testPanelShowAndHide = async ({
    target, panelId, isFirstPanelLoad, expectedResults,
  }) => {
    info("Wait Addon Devtools Panel to be shown");

    await gDevTools.showToolbox(target, panelId);
    const {devtoolsPageTabId} = await extension.awaitMessage("devtools_panel_shown");

    // If the panel is loaded for the first time, we expect to also
    // receive the test messages and assert that both the page and the panel
    // have the same devtools.inspectedWindow.tabId value.
    if (isFirstPanelLoad) {
      const devtoolsPanelTabId = await extension.awaitMessage("devtools_panel_inspectedWindow_tabId");
      is(devtoolsPanelTabId, devtoolsPageTabId,
         "Got the same devtools.inspectedWindow.tabId from devtools page and panel");
    }

    info("Wait Addon Devtools Panel to be shown");

    await gDevTools.showToolbox(target, "testBlankPanel");
    const results = await extension.awaitMessage("devtools_panel_hidden");

    // We already checked the tabId, remove it from the results, so that we can check
    // the remaining properties using a single Assert.deepEqual.
    delete results.devtoolsPageTabId;

    Assert.deepEqual(
      results, expectedResults,
      "Got the expected number of created panels and shown/hidden events"
    );
  };

  // Test the extension devtools_page enabling/disabling through the related
  // about:config preference.
  const testExtensionDevToolsPref = async ({prefValue, toolbox, oldPanelId}) => {
    if (!prefValue) {
      // Test that the extension devtools_page is shutting down when the related
      // about:config preference has been set to false, and the panel on its left
      // is being selected.
      info("Turning off the extension devtools page from its about:config preference");
      let waitToolSelected = toolbox.once("select");
      Services.prefs.setBoolPref(extensionPrefName, false);
      const selectedTool = await waitToolSelected;
      isnot(selectedTool, oldPanelId, "Expect a different panel to be selected");

      let toolboxAdditionalTools = toolbox.getAdditionalTools();
      is(toolboxAdditionalTools.length, 0, "Extension devtools panel unregistered");
      is(toolbox.visibleAdditionalTools.filter(toolId => toolId == oldPanelId).length, 0,
         "Removed panel should not be listed in the visible additional tools");
    } else {
      // Test that the extension devtools_page and panel are being created again when
      // the related about:config preference has been set to true.
      info("Turning on the extension devtools page from its about:config preference");
      Services.prefs.setBoolPref(extensionPrefName, true);
      await extension.awaitMessage("devtools_panel_created");

      let toolboxAdditionalTools = toolbox.getAdditionalTools();
      is(toolboxAdditionalTools.length, 1, "Got one extension devtools panel registered");

      let newPanelId = getPanelId(toolbox);
      is(toolbox.visibleAdditionalTools.filter(toolId => toolId == newPanelId).length, 1,
         "Extension panel is listed in the visible additional tools");
    }
  };

  // Wait that the devtools_page has created its devtools panel and retrieve its
  // panel id.
  let {toolbox, target} = await openToolboxForTab(tab);
  await extension.awaitMessage("devtools_panel_created");
  let panelId = getPanelId(toolbox);

  info("Test panel show and hide - first cycle");
  await testPanelShowAndHide({
    target, panelId,
    isFirstPanelLoad: true,
    expectedResults: {
      panelCreated: 1,
      panelShown: 1,
      panelHidden: 1,
    },
  });

  info("Test panel show and hide - second cycle");
  await testPanelShowAndHide({
    target, panelId,
    isFirstPanelLoad: false,
    expectedResults: {
      panelCreated: 1,
      panelShown: 2,
      panelHidden: 2,
    },
  });

  // Go back to the extension devtools panel.
  await gDevTools.showToolbox(target, panelId);
  await extension.awaitMessage("devtools_panel_shown");

  // Turn off the extension devtools page using the preference that enable/disable the
  // devtools page for a given installed WebExtension.
  await testExtensionDevToolsPref({
    toolbox,
    prefValue: false,
    oldPanelId: panelId,
  });

  // Close and Re-open the toolbox to verify that the toolbox doesn't load the
  // devtools_page and the devtools panel.
  info("Re-open the toolbox and expect no extension devtools panel");
  await closeToolboxForTab(tab);
  ({toolbox, target} = await openToolboxForTab(tab));

  let toolboxAdditionalTools = toolbox.getAdditionalTools();
  is(toolboxAdditionalTools.length, 0,
     "Got no extension devtools panel on the opened toolbox as expected.");

  // Close and Re-open the toolbox to verify that the toolbox does load the
  // devtools_page and the devtools panel again.
  info("Restart the toolbox and enable the extension devtools panel");
  await closeToolboxForTab(tab);
  ({toolbox, target} = await openToolboxForTab(tab));

  // Turn the addon devtools panel back on using the preference that enable/disable the
  // devtools page for a given installed WebExtension.
  await testExtensionDevToolsPref({
    toolbox,
    prefValue: true,
  });

  // Test devtools panel is loaded correctly after being toggled and
  // devtools panel events has been fired as expected.
  panelId = getPanelId(toolbox);

  info("Test panel show and hide - after disabling/enabling devtools_page");
  await testPanelShowAndHide({
    target, panelId,
    isFirstPanelLoad: true,
    expectedResults: {
      panelCreated: 1,
      panelShown: 1,
      panelHidden: 1,
    },
  });

  await closeToolboxForTab(tab);

  await extension.unload();

  // Verify that the extension preference branch has been removed once the extension
  // has been uninstalled.
  prefBranch = Services.prefs.getBranch(extensionPrefBranch);
  is(prefBranch.getPrefType("enabled"), prefBranch.PREF_INVALID,
     "The preference branch for the extension should have been removed");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_devtools_page_panels_switch_toolbox_host() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function devtools_panel() {
    const hasDevToolsAPINamespace = "devtools" in browser;

    browser.test.sendMessage("devtools_panel_loaded", {
      hasDevToolsAPINamespace,
      panelLoadedURL: window.location.href,
    });
  }

  async function devtools_page() {
    const panel = await browser.devtools.panels.create(
      "Test Panel Switch Host", "fake-icon.png", "devtools_panel.html"
    );

    panel.onShown.addListener(panelWindow => {
      browser.test.sendMessage("devtools_panel_shown", panelWindow.location.href);
    });

    panel.onHidden.addListener(() => {
      browser.test.sendMessage("devtools_panel_hidden");
    });

    browser.test.sendMessage("devtools_panel_created");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": createPage("devtools_page.js"),
      "devtools_page.js": devtools_page,
      "devtools_panel.html": createPage("devtools_panel.js", "DEVTOOLS PANEL"),
      "devtools_panel.js": devtools_panel,
    },
  });

  await extension.startup();

  let {toolbox, target} = await openToolboxForTab(tab);
  await extension.awaitMessage("devtools_panel_created");

  const toolboxAdditionalTools = toolbox.getAdditionalTools();

  is(toolboxAdditionalTools.length, 1,
     "Got the expected number of toolbox specific panel registered.");

  const panelDef = toolboxAdditionalTools[0];
  const panelId = panelDef.id;

  info("Selecting the addon devtools panel");
  await gDevTools.showToolbox(target, panelId);

  info("Wait for the panel to show and load for the first time");
  const panelShownURL = await extension.awaitMessage("devtools_panel_shown");

  const {
    panelLoadedURL,
    hasDevToolsAPINamespace,
  } = await extension.awaitMessage("devtools_panel_loaded");

  is(panelShownURL, panelLoadedURL, "Got the expected panel URL on the first load");
  ok(hasDevToolsAPINamespace, "The devtools panel has the devtools API on the first load");

  const originalToolboxHostType = toolbox.hostType;

  info("Switch the toolbox from docked on bottom to docked on side");
  toolbox.switchHost("side");

  info("Wait for the panel to emit hide, show and load messages once docked on side");
  await extension.awaitMessage("devtools_panel_hidden");
  const dockedOnSideShownURL = await extension.awaitMessage("devtools_panel_shown");

  is(dockedOnSideShownURL, panelShownURL,
     "Got the expected panel url once the panel shown event has been emitted on toolbox host changed");

  const dockedOnSideLoaded = await extension.awaitMessage("devtools_panel_loaded");

  is(dockedOnSideLoaded.panelLoadedURL, panelShownURL,
     "Got the expected panel url once the panel has been reloaded on toolbox host changed");
  ok(dockedOnSideLoaded.hasDevToolsAPINamespace,
     "The devtools panel has the devtools API once the toolbox host has been changed");

  info("Switch the toolbox from docked on bottom to the original dock mode");
  toolbox.switchHost(originalToolboxHostType);

  info("Wait for the panel test messages once toolbox dock mode has been restored");
  await extension.awaitMessage("devtools_panel_hidden");
  await extension.awaitMessage("devtools_panel_shown");
  await extension.awaitMessage("devtools_panel_loaded");

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_devtools_page_invalid_panel_urls() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function devtools_page() {
    const matchInvalidPanelURL = /must be a relative URL/;
    const matchInvalidIconURL = /be one of \[""\], or match the format "strictRelativeUrl"/;

    // Invalid panel urls (validated by the schema wrappers, throws on invalid urls).
    const invalid_panels = [
      {panel: "about:about", icon: "icon.png", expectError: matchInvalidPanelURL},
      {panel: "about:addons", icon: "icon.png", expectError: matchInvalidPanelURL},
      {panel: "http://mochi.test:8888", icon: "icon.png", expectError: matchInvalidPanelURL},
      // Invalid icon urls (validated inside the API method because of the empty icon string
      // which have to be resolved to the default icon, reject the returned promise).
      {panel: "panel.html", icon: "about:about", expectError: matchInvalidIconURL},
      {panel: "panel.html", icon: "http://mochi.test:8888", expectError: matchInvalidIconURL},
    ];

    const valid_panels = [
      {panel: "panel.html", icon: "icon.png"},
      {panel: "./panel.html", icon: "icon.png"},
      {panel: "/panel.html", icon: "icon.png"},
      {panel: "/panel.html", icon: ""},
    ];

    let valid_panels_length = valid_panels.length;

    const test_cases = [].concat(invalid_panels, valid_panels);

    browser.test.onMessage.addListener(async msg => {
      if (msg !== "start_test_panel_create") {
        return;
      }

      for (let {panel, icon, expectError} of test_cases) {
        browser.test.log(`Testing devtools.panels.create for ${JSON.stringify({panel, icon})}`);

        if (expectError) {
          // Verify that invalid panel urls throw.
          browser.test.assertThrows(
            () => browser.devtools.panels.create("Test Panel", icon, panel),
            expectError,
            "Got the expected rejection on creating a devtools panel with " +
            `panel url ${panel} and icon ${icon}`
          );
        } else {
          // Verify that with valid panel and icon urls the panel is created and loaded
          // as expected.
          try {
            const pane = await browser.devtools.panels.create("Test Panel", icon, panel);

            valid_panels_length--;

            // Wait the panel to be loaded.
            const oncePanelLoaded = new Promise(resolve => {
              pane.onShown.addListener(paneWin => {
                browser.test.assertTrue(
                  paneWin.location.href.endsWith("/panel.html"),
                  `The panel has loaded the expected extension URL with ${panel}`);
                resolve();
              });
            });

            // Ask the privileged code to select the last created panel.
            const done = valid_panels_length === 0;
            browser.test.sendMessage("select-devtools-panel", done);
            await oncePanelLoaded;
          } catch (err) {
            browser.test.fail("Unexpected failure on creating a devtools panel with " +
                              `panel url ${panel} and icon ${icon}`);
            throw err;
          }
        }
      }

      browser.test.sendMessage("test_invalid_devtools_panel_urls_done");
    });

    browser.test.sendMessage("devtools_page_ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      devtools_page: "devtools_page.html",
      icons: {
        "32": "icon.png",
      },
    },
    files: {
      "devtools_page.html": createPage("devtools_page.js"),
      "devtools_page.js": devtools_page,
      "panel.html": createPage("panel.js", "DEVTOOLS PANEL"),
      "panel.js": "",
      "icon.png": imageBuffer,
      "default-icon.png": imageBuffer,
    },
  });

  await extension.startup();

  let {toolbox, target} = await openToolboxForTab(tab);
  info("developer toolbox opened");

  await extension.awaitMessage("devtools_page_ready");

  extension.sendMessage("start_test_panel_create");

  let done = false;

  while (!done) {
    info("Waiting test extension request to select the last created panel");
    done = await extension.awaitMessage("select-devtools-panel");

    const toolboxAdditionalTools = toolbox.getAdditionalTools();
    const lastTool = toolboxAdditionalTools[toolboxAdditionalTools.length - 1];

    gDevTools.showToolbox(target, lastTool.id);
    info("Last created panel selected");
  }

  await extension.awaitMessage("test_invalid_devtools_panel_urls_done");

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
