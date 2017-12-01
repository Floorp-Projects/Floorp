/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for splitting";
const {Toolbox} = require("devtools/client/framework/toolbox");

// Test is slow on Linux EC2 instances - Bug 962931
requestLongerTimeout(2);

add_task(async function () {
  let toolbox;

  await addTab(TEST_URI);
  await testConsoleLoadOnDifferentPanel();
  await testKeyboardShortcuts();
  await checkAllTools();

  info("Testing host types");
  checkHostType(Toolbox.HostType.BOTTOM);
  checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.SIDE);
  checkHostType(Toolbox.HostType.SIDE);
  checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.WINDOW);
  checkHostType(Toolbox.HostType.WINDOW);
  checkToolboxUI();
  await toolbox.switchHost(Toolbox.HostType.BOTTOM);

  async function testConsoleLoadOnDifferentPanel() {
    info("About to check console loads even when non-webconsole panel is open");

    await openPanel("inspector");
    let webconsoleReady = toolbox.once("webconsole-ready");
    toolbox.toggleSplitConsole();
    await webconsoleReady;
    ok(true, "Webconsole has been triggered as loaded while another tool is active");
  }

  async function testKeyboardShortcuts() {
    info("About to check that panel responds to ESCAPE keyboard shortcut");

    let splitConsoleReady = toolbox.once("split-console");
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

  function getCurrentUIState() {
    let deck = toolbox.doc.querySelector("#toolbox-deck");
    let webconsolePanel = toolbox.webconsolePanel;
    let splitter = toolbox.doc.querySelector("#toolbox-console-splitter");

    let containerHeight = deck.parentNode.getBoundingClientRect().height;
    let deckHeight = deck.getBoundingClientRect().height;
    let webconsoleHeight = webconsolePanel.getBoundingClientRect().height;
    let splitterVisibility = !splitter.getAttribute("hidden");
    let openedConsolePanel = toolbox.currentToolId === "webconsole";
    let cmdButton = toolbox.doc.querySelector("#command-button-splitconsole");

    return {
      deckHeight: deckHeight,
      containerHeight: containerHeight,
      webconsoleHeight: webconsoleHeight,
      splitterVisibility: splitterVisibility,
      openedConsolePanel: openedConsolePanel,
      buttonSelected: cmdButton.classList.contains("checked")
    };
  }

  async function checkWebconsolePanelOpened() {
    info("About to check special cases when webconsole panel is open.");

    // Start with console split, so we can test for transition to main panel.
    await toolbox.toggleSplitConsole();

    let currentUIState = getCurrentUIState();

    ok(currentUIState.splitterVisibility,
       "Splitter is visible when console is split");
    ok(currentUIState.deckHeight > 0,
       "Deck has a height > 0 when console is split");
    ok(currentUIState.webconsoleHeight > 0,
       "Web console has a height > 0 when console is split");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    ok(currentUIState.buttonSelected, "The command button is selected");

    await openPanel("webconsole");
    currentUIState = getCurrentUIState();

    ok(!currentUIState.splitterVisibility,
       "Splitter is hidden when console is opened.");
    is(currentUIState.deckHeight, 0,
       "Deck has a height == 0 when console is opened.");
    is(currentUIState.webconsoleHeight, currentUIState.containerHeight,
       "Web console is full height.");
    ok(currentUIState.openedConsolePanel,
       "The console panel is the current tool");
    ok(currentUIState.buttonSelected,
       "The command button is still selected.");

    // Make sure splitting console does nothing while webconsole is opened
    await toolbox.toggleSplitConsole();

    currentUIState = getCurrentUIState();

    ok(!currentUIState.splitterVisibility,
       "Splitter is hidden when console is opened.");
    is(currentUIState.deckHeight, 0,
       "Deck has a height == 0 when console is opened.");
    is(currentUIState.webconsoleHeight, currentUIState.containerHeight,
       "Web console is full height.");
    ok(currentUIState.openedConsolePanel,
       "The console panel is the current tool");
    ok(currentUIState.buttonSelected,
       "The command button is still selected.");

    // Make sure that split state is saved after opening another panel
    await openPanel("inspector");
    currentUIState = getCurrentUIState();
    ok(currentUIState.splitterVisibility,
       "Splitter is visible when console is split");
    ok(currentUIState.deckHeight > 0,
       "Deck has a height > 0 when console is split");
    ok(currentUIState.webconsoleHeight > 0,
       "Web console has a height > 0 when console is split");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    ok(currentUIState.buttonSelected,
       "The command button is still selected.");

    await toolbox.toggleSplitConsole();
  }

  async function checkToolboxUI() {
    let currentUIState = getCurrentUIState();

    ok(!currentUIState.splitterVisibility, "Splitter is hidden by default");
    is(currentUIState.deckHeight, currentUIState.containerHeight,
       "Deck has a height > 0 by default");
    is(currentUIState.webconsoleHeight, 0,
       "Web console is collapsed by default");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    ok(!currentUIState.buttonSelected, "The command button is not selected.");

    await toolbox.toggleSplitConsole();

    currentUIState = getCurrentUIState();

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
    ok(currentUIState.buttonSelected, "The command button is selected.");

    await toolbox.toggleSplitConsole();

    currentUIState = getCurrentUIState();

    ok(!currentUIState.splitterVisibility, "Splitter is hidden after toggling");
    is(currentUIState.deckHeight, currentUIState.containerHeight,
       "Deck has a height > 0 after toggling");
    is(currentUIState.webconsoleHeight, 0,
       "Web console is collapsed after toggling");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    ok(!currentUIState.buttonSelected, "The command button is not selected.");
  }

  async function openPanel(toolId) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    toolbox = await gDevTools.showToolbox(target, toolId);
  }

  async function openAndCheckPanel(toolId) {
    await openPanel(toolId);
    await checkToolboxUI(toolbox.getCurrentPanel());
  }

  function checkHostType(hostType) {
    is(toolbox.hostType, hostType, "host type is " + hostType);

    let pref = Services.prefs.getCharPref("devtools.toolbox.host");
    is(pref, hostType, "host pref is " + hostType);
  }
});
