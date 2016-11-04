/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for splitting";

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({"set": [["dom.ipc.processCount", 1]]}, runTest);
}

function runTest() {
  // Test is slow on Linux EC2 instances - Bug 962931
  requestLongerTimeout(2);

  let {Toolbox} = require("devtools/client/framework/toolbox");
  let toolbox;

  loadTab(TEST_URI).then(testConsoleLoadOnDifferentPanel);

  function testConsoleLoadOnDifferentPanel() {
    info("About to check console loads even when non-webconsole panel is open");

    openPanel("inspector").then(() => {
      toolbox.on("webconsole-ready", () => {
        ok(true, "Webconsole has been triggered as loaded while another tool " +
                 "is active");
        testKeyboardShortcuts();
      });

      // Opens split console.
      toolbox.toggleSplitConsole();
    });
  }

  function testKeyboardShortcuts() {
    info("About to check that panel responds to ESCAPE keyboard shortcut");

    toolbox.once("split-console", () => {
      ok(true, "Split console has been triggered via ESCAPE keypress");
      checkAllTools();
    });

    // Closes split console.
    EventUtils.sendKey("ESCAPE", toolbox.win);
  }

  function checkAllTools() {
    info("About to check split console with each panel individually.");

    Task.spawn(function* () {
      yield openAndCheckPanel("jsdebugger");
      yield openAndCheckPanel("inspector");
      yield openAndCheckPanel("styleeditor");
      yield openAndCheckPanel("performance");
      yield openAndCheckPanel("netmonitor");

      yield checkWebconsolePanelOpened();
      testBottomHost();
    });
  }

  function getCurrentUIState() {
    let win = toolbox.win;
    let deck = toolbox.doc.querySelector("#toolbox-deck");
    let webconsolePanel = toolbox.webconsolePanel;
    let splitter = toolbox.doc.querySelector("#toolbox-console-splitter");

    let containerHeight = parseFloat(win.getComputedStyle(deck.parentNode)
      .getPropertyValue("height"));
    let deckHeight = parseFloat(win.getComputedStyle(deck)
      .getPropertyValue("height"));
    let webconsoleHeight = parseFloat(win.getComputedStyle(webconsolePanel)
      .getPropertyValue("height"));
    let splitterVisibility = !splitter.getAttribute("hidden");
    let openedConsolePanel = toolbox.currentToolId === "webconsole";
    let cmdButton = toolbox.doc.querySelector("#command-button-splitconsole");

    return {
      deckHeight: deckHeight,
      containerHeight: containerHeight,
      webconsoleHeight: webconsoleHeight,
      splitterVisibility: splitterVisibility,
      openedConsolePanel: openedConsolePanel,
      buttonSelected: cmdButton.hasAttribute("checked")
    };
  }

  function checkWebconsolePanelOpened() {
    info("About to check special cases when webconsole panel is open.");

    let deferred = promise.defer();

    // Start with console split, so we can test for transition to main panel.
    toolbox.toggleSplitConsole();

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

    openPanel("webconsole").then(() => {
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
      toolbox.toggleSplitConsole();

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
      openPanel("inspector").then(() => {
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

        toolbox.toggleSplitConsole();
        deferred.resolve();
      });
    });
    return deferred.promise;
  }

  function openPanel(toolId) {
    let deferred = promise.defer();
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, toolId).then(function (box) {
      toolbox = box;
      deferred.resolve();
    }).then(null, console.error);
    return deferred.promise;
  }

  function openAndCheckPanel(toolId) {
    let deferred = promise.defer();
    openPanel(toolId).then(() => {
      info("Checking toolbox for " + toolId);
      checkToolboxUI(toolbox.getCurrentPanel());
      deferred.resolve();
    });
    return deferred.promise;
  }

  function checkToolboxUI() {
    let currentUIState = getCurrentUIState();

    ok(!currentUIState.splitterVisibility, "Splitter is hidden by default");
    is(currentUIState.deckHeight, currentUIState.containerHeight,
       "Deck has a height > 0 by default");
    is(currentUIState.webconsoleHeight, 0,
       "Web console is collapsed by default");
    ok(!currentUIState.openedConsolePanel,
       "The console panel is not the current tool");
    ok(!currentUIState.buttonSelected, "The command button is not selected.");

    toolbox.toggleSplitConsole();

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

    toolbox.toggleSplitConsole();

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

  function testBottomHost() {
    checkHostType(Toolbox.HostType.BOTTOM);

    checkToolboxUI();

    toolbox.switchHost(Toolbox.HostType.SIDE).then(testSidebarHost);
  }

  function testSidebarHost() {
    checkHostType(Toolbox.HostType.SIDE);

    checkToolboxUI();

    toolbox.switchHost(Toolbox.HostType.WINDOW).then(testWindowHost);
  }

  function testWindowHost() {
    checkHostType(Toolbox.HostType.WINDOW);

    checkToolboxUI();

    toolbox.switchHost(Toolbox.HostType.BOTTOM).then(testDestroy);
  }

  function checkHostType(hostType) {
    is(toolbox.hostType, hostType, "host type is " + hostType);

    let pref = Services.prefs.getCharPref("devtools.toolbox.host");
    is(pref, hostType, "host pref is " + hostType);
  }

  function testDestroy() {
    toolbox.destroy().then(function () {
      let target = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target).then(finish);
    });
  }

  function finish() {
    toolbox = null;
    finishTest();
  }
}
