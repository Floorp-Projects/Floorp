/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for splitting";
const {Toolbox} = require("devtools/client/framework/toolbox");
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N =
  new LocalizationHelper("devtools/client/locales/toolbox.properties");

// Test is slow on Linux EC2 instances - Bug 962931
requestLongerTimeout(2);

add_task(async function() {
  let toolbox;

  await addTab(TEST_URI);
  await testConsoleLoadOnDifferentPanel();
  await testKeyboardShortcuts();
  await checkAllTools();

  info("Testing host types");
  checkHostType(Toolbox.HostType.BOTTOM);
  await checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.RIGHT);
  checkHostType(Toolbox.HostType.RIGHT);
  await checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.WINDOW);

  // checkHostType, below,  will open the meatball menu to read the "Split
  // console" menu item label. However, if we've just opened a new window then
  // on some platforms when we switch focus to the new window we might end up
  // triggering the auto-close behavior on the menu popup. To avoid that, wait
  // a moment before querying the menu.
  await new Promise(resolve => requestIdleCallback(resolve));

  checkHostType(Toolbox.HostType.WINDOW);
  await checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.BOTTOM);

  async function testConsoleLoadOnDifferentPanel() {
    info("About to check console loads even when non-webconsole panel is open");

    await openPanel("inspector");
    const webconsoleReady = toolbox.once("webconsole-ready");
    await toolbox.toggleSplitConsole();
    await webconsoleReady;
    ok(true, "Webconsole has been triggered as loaded while another tool is active");
  }

  async function testKeyboardShortcuts() {
    info("About to check that panel responds to ESCAPE keyboard shortcut");

    const splitConsoleReady = toolbox.once("split-console");
    EventUtils.sendKey("ESCAPE", toolbox.win);
    await splitConsoleReady;
    ok(true, "Split console has been triggered via ESCAPE keypress");
  }

  async function checkAllTools() {
    info("About to check split console with each panel individually.");
    await openAndCheckPanel("jsdebugger");
    await openAndCheckPanel("inspector");
    await openAndCheckPanel("styleeditor");
    await openAndCheckPanel("performance");
    await openAndCheckPanel("netmonitor");

    await checkWebconsolePanelOpened();
  }

  async function getCurrentUIState() {
    const deck = toolbox.doc.querySelector("#toolbox-deck");
    const webconsolePanel = toolbox.webconsolePanel;
    const splitter = toolbox.doc.querySelector("#toolbox-console-splitter");

    const containerHeight = deck.parentNode.getBoundingClientRect().height;
    const deckHeight = deck.getBoundingClientRect().height;
    const webconsoleHeight = webconsolePanel.getBoundingClientRect().height;
    const splitterVisibility = !splitter.getAttribute("hidden");
    const openedConsolePanel = toolbox.currentToolId === "webconsole";
    const menuLabel = await getMenuLabel(toolbox);

    return {
      deckHeight: deckHeight,
      containerHeight: containerHeight,
      webconsoleHeight: webconsoleHeight,
      splitterVisibility: splitterVisibility,
      openedConsolePanel: openedConsolePanel,
      menuLabel,
    };
  }

  function getMenuLabel() {
    return new Promise(resolve => {
      const button = toolbox.doc.getElementById("toolbox-meatball-menu-button");
      EventUtils.sendMouseEvent({ type: "click" }, button);

      toolbox.doc.addEventListener("popupshown", () => {
        const menuItem =
          toolbox.doc.getElementById("toolbox-meatball-menu-splitconsole");

        // Return undefined if the menu item is not available
        let label;
        if (menuItem && menuItem.querySelector(".label")) {
          label =
            menuItem.querySelector(".label").textContent ===
            L10N.getStr("toolbox.meatballMenu.hideconsole.label")
              ? "hide"
              : "split";
        }

        // Wait for menu to close
        toolbox.doc.addEventListener("popuphidden", () => {
          resolve(label);
        }, { once: true });
        EventUtils.sendKey("ESCAPE", toolbox.win);
      }, { once: true });
    });
  }

  async function checkWebconsolePanelOpened() {
    info("About to check special cases when webconsole panel is open.");

    // Start with console split, so we can test for transition to main panel.
    await toolbox.toggleSplitConsole();

    let currentUIState = await getCurrentUIState();

    ok(currentUIState.splitterVisibility,
       "Splitter is visible when console is split");
    ok(currentUIState.deckHeight > 0,
       "Deck has a height > 0 when console is split");
    ok(currentUIState.webconsoleHeight > 0,
       "Web console has a height > 0 when console is split");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    is(currentUIState.menuLabel, "hide",
       "The menu item indicates the console is split");

    await openPanel("webconsole");
    currentUIState = await getCurrentUIState();

    ok(!currentUIState.splitterVisibility,
       "Splitter is hidden when console is opened.");
    is(currentUIState.deckHeight, 0,
       "Deck has a height == 0 when console is opened.");
    is(currentUIState.webconsoleHeight, currentUIState.containerHeight,
       "Web console is full height.");
    ok(currentUIState.openedConsolePanel,
       "The console panel is the current tool");
    is(currentUIState.menuLabel, undefined,
       "The menu item is hidden when console is opened");

    // Make sure splitting console does nothing while webconsole is opened
    await toolbox.toggleSplitConsole();

    currentUIState = await getCurrentUIState();

    ok(!currentUIState.splitterVisibility,
       "Splitter is hidden when console is opened.");
    is(currentUIState.deckHeight, 0,
       "Deck has a height == 0 when console is opened.");
    is(currentUIState.webconsoleHeight, currentUIState.containerHeight,
       "Web console is full height.");
    ok(currentUIState.openedConsolePanel,
       "The console panel is the current tool");
    is(currentUIState.menuLabel, undefined,
       "The menu item is hidden when console is opened");

    // Make sure that split state is saved after opening another panel
    await openPanel("inspector");
    currentUIState = await getCurrentUIState();
    ok(currentUIState.splitterVisibility,
       "Splitter is visible when console is split");
    ok(currentUIState.deckHeight > 0,
       "Deck has a height > 0 when console is split");
    ok(currentUIState.webconsoleHeight > 0,
       "Web console has a height > 0 when console is split");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    is(currentUIState.menuLabel, "hide",
       "The menu item still indicates the console is split");

    await toolbox.toggleSplitConsole();
  }

  async function checkToolboxUI() {
    let currentUIState = await getCurrentUIState();

    ok(!currentUIState.splitterVisibility, "Splitter is hidden by default");
    is(currentUIState.deckHeight, currentUIState.containerHeight,
       "Deck has a height > 0 by default");
    is(currentUIState.webconsoleHeight, 0,
       "Web console is collapsed by default");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    is(currentUIState.menuLabel, "split",
       "The menu item indicates the console is not split");

    await toolbox.toggleSplitConsole();

    currentUIState = await getCurrentUIState();

    ok(currentUIState.splitterVisibility,
       "Splitter is visible when console is split");
    ok(currentUIState.deckHeight > 0,
       "Deck has a height > 0 when console is split");
    ok(currentUIState.webconsoleHeight > 0,
       "Web console has a height > 0 when console is split");
    is(Math.round(currentUIState.deckHeight + currentUIState.webconsoleHeight),
       currentUIState.containerHeight,
       "Everything adds up to container height");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    is(currentUIState.menuLabel, "hide",
       "The menu item indicates the console is split");

    await toolbox.toggleSplitConsole();

    currentUIState = await getCurrentUIState();

    ok(!currentUIState.splitterVisibility, "Splitter is hidden after toggling");
    is(currentUIState.deckHeight, currentUIState.containerHeight,
       "Deck has a height > 0 after toggling");
    is(currentUIState.webconsoleHeight, 0,
       "Web console is collapsed after toggling");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    is(currentUIState.menuLabel, "split",
       "The menu item indicates the console is not split");
  }

  async function openPanel(toolId) {
    const target = TargetFactory.forTab(gBrowser.selectedTab);
    toolbox = await gDevTools.showToolbox(target, toolId);
  }

  async function openAndCheckPanel(toolId) {
    await openPanel(toolId);
    await checkToolboxUI(toolbox.getCurrentPanel());
  }

  function checkHostType(hostType) {
    is(toolbox.hostType, hostType, "host type is " + hostType);

    const pref = Services.prefs.getCharPref("devtools.toolbox.host");
    is(pref, hostType, "host pref is " + hostType);
  }
});
