/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let TEST_URL = "data:text/html;charset=utf8,test for dynamically " +
               "registering and unregistering tools";

// The frames button is only shown if the page has at least one iframe so we
// need to add one to the test page.
TEST_URL += "<iframe src=\"data:text/plain,iframe\"></iframe>";

var doc = null, toolbox = null, panelWin = null, modifiedPrefs = [];

function test() {
  addTab(TEST_URL).then(tab => {
    const target = TargetFactory.forTab(tab);
    gDevTools.showToolbox(target)
      .then(testSelectTool)
      .then(testToggleToolboxButtons)
      .then(testPrefsAreRespectedWhenReopeningToolbox)
      .then(cleanup, errorHandler);
  });
}

function testPrefsAreRespectedWhenReopeningToolbox() {
  const target = TargetFactory.forTab(gBrowser.selectedTab);

  return new Promise(resolve => {
    info("Closing toolbox to test after reopening");
    gDevTools.closeToolbox(target).then(() => {
      const tabTarget = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(tabTarget)
        .then(testSelectTool)
        .then(() => {
          info("Toolbox has been reopened.  Checking UI state.");
          testPreferenceAndUIStateIsConsistent();
          resolve();
        });
    });
  });
}

function testSelectTool(devtoolsToolbox) {
  return new Promise(resolve => {
    info("Selecting the options panel");

    toolbox = devtoolsToolbox;
    doc = toolbox.doc;

    toolbox.selectTool("options");
    toolbox.once("options-selected", tool => {
      ok(true, "Options panel selected via selectTool method");
      panelWin = tool.panelWin;
      resolve();
    });
  });
}

function testPreferenceAndUIStateIsConsistent() {
  const checkNodes = [...panelWin.document.querySelectorAll(
    "#enabled-toolbox-buttons-box input[type=checkbox]")];
  const toolboxButtonNodes = [...doc.querySelectorAll(".command-button")];

  for (const tool of toolbox.toolbarButtons) {
    const isVisible = getBoolPref(tool.visibilityswitch);

    const button = toolboxButtonNodes.find(toolboxButton => toolboxButton.id === tool.id);
    is(!!button, isVisible,
      "Button visibility matches pref for " + tool.id);

    const check = checkNodes.filter(node => node.id === tool.id)[0];
    is(check.checked, isVisible,
      "Checkbox should be selected based on current pref for " + tool.id);
  }
}

function testToggleToolboxButtons() {
  const checkNodes = [...panelWin.document.querySelectorAll(
    "#enabled-toolbox-buttons-box input[type=checkbox]")];

  const visibleToolbarButtons = toolbox.toolbarButtons.filter(tool => tool.isVisible);

  const toolbarButtonNodes = [...doc.querySelectorAll(".command-button")];

  is(checkNodes.length, toolbox.toolbarButtons.length,
    "All of the buttons are toggleable.");
  is(visibleToolbarButtons.length, toolbarButtonNodes.length,
    "All of the DOM buttons are toggleable.");

  for (const tool of toolbox.toolbarButtons) {
    const id = tool.id;
    const matchedCheckboxes = checkNodes.filter(node => node.id === id);
    const matchedButtons = toolbarButtonNodes.filter(button => button.id === id);
    is(matchedCheckboxes.length, 1,
      "There should be a single toggle checkbox for: " + id);
    if (tool.isVisible) {
      is(matchedButtons.length, 1,
        "There should be a DOM button for the visible: " + id);
      is(matchedButtons[0].getAttribute("title"), tool.description,
        "The tooltip for button matches the tool definition.");
    } else {
      is(matchedButtons.length, 0,
        "There should not be a DOM button for the invisible: " + id);
    }

    is(matchedCheckboxes[0].nextSibling.textContent, tool.description,
      "The label for checkbox matches the tool definition.");
  }

  // Store modified pref names so that they can be cleared on error.
  for (const tool of toolbox.toolbarButtons) {
    const pref = tool.visibilityswitch;
    modifiedPrefs.push(pref);
  }

  // Try checking each checkbox, making sure that it changes the preference
  for (const node of checkNodes) {
    const tool = toolbox.toolbarButtons.filter(
      commandButton => commandButton.id === node.id)[0];
    const isVisible = getBoolPref(tool.visibilityswitch);

    testPreferenceAndUIStateIsConsistent();
    node.click();
    testPreferenceAndUIStateIsConsistent();

    const isVisibleAfterClick = getBoolPref(tool.visibilityswitch);

    is(isVisible, !isVisibleAfterClick,
      "Clicking on the node should have toggled visibility preference for " +
      tool.visibilityswitch);
  }

  return promise.resolve();
}

function getBoolPref(key) {
  return Services.prefs.getBoolPref(key);
}

function cleanup() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
    for (const pref of modifiedPrefs) {
      Services.prefs.clearUserPref(pref);
    }
    toolbox = doc = panelWin = modifiedPrefs = null;
    finish();
  });
}

function errorHandler(error) {
  ok(false, "Unexpected error: " + error);
  cleanup();
}
