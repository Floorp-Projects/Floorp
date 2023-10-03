/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let TEST_URL =
  "data:text/html;charset=utf8,test for dynamically " +
  "registering and unregistering tools";

// The frames button is only shown if the page has at least one iframe so we
// need to add one to the test page.
TEST_URL += '<iframe src="data:text/plain,iframe"></iframe>';
// The error count button is only shown if there are errors on the page
TEST_URL += '<script>console.error("err")</script>';

var modifiedPrefs = [];
registerCleanupFunction(() => {
  for (const pref of modifiedPrefs) {
    Services.prefs.clearUserPref(pref);
  }
});

const TOGGLE_BUTTONS = [
  "command-button-measure",
  "command-button-rulers",
  "command-button-responsive",
  "command-button-pick",
];

add_task(async function test() {
  const tab = await addTab(TEST_URL);
  let toolbox = await gDevTools.showToolboxForTab(tab);
  const optionsPanelWin = await selectOptionsPanel(toolbox);
  await testToggleToolboxButtons(toolbox, optionsPanelWin);
  toolbox = await testPrefsAreRespectedWhenReopeningToolbox();
  await testButtonStateOnClick(toolbox);

  await toolbox.destroy();
});

async function selectOptionsPanel(toolbox) {
  info("Selecting the options panel");

  const onOptionsSelected = toolbox.once("options-selected");
  toolbox.selectTool("options");
  const optionsPanel = await onOptionsSelected;
  ok(true, "Options panel selected via selectTool method");
  return optionsPanel.panelWin;
}

async function testToggleToolboxButtons(toolbox, optionsPanelWin) {
  const checkNodes = [
    ...optionsPanelWin.document.querySelectorAll(
      "#enabled-toolbox-buttons-box input[type=checkbox]"
    ),
  ];

  // Filter out all the buttons which are not supported on the current target.
  // (DevTools Experimental Preferences etc...)
  const toolbarButtons = toolbox.toolbarButtons.filter(tool =>
    tool.isToolSupported(toolbox)
  );

  const visibleToolbarButtons = toolbarButtons.filter(tool => tool.isVisible);

  const toolbarButtonNodes = [
    ...toolbox.doc.querySelectorAll(".command-button"),
  ];

  is(
    checkNodes.length,
    toolbarButtons.length,
    "All of the buttons are toggleable."
  );
  is(
    visibleToolbarButtons.length,
    toolbarButtonNodes.length,
    "All of the DOM buttons are toggleable."
  );

  for (const tool of toolbarButtons) {
    const id = tool.id;
    const matchedCheckboxes = checkNodes.filter(node => node.id === id);
    const matchedButtons = toolbarButtonNodes.filter(
      button => button.id === id
    );
    if (tool.isVisible) {
      is(
        matchedCheckboxes.length,
        1,
        "There should be a single toggle checkbox for: " + id
      );
      is(
        matchedCheckboxes[0].nextSibling.textContent,
        tool.description,
        "The label for checkbox matches the tool definition."
      );
      is(
        matchedButtons.length,
        1,
        "There should be a DOM button for the visible: " + id
      );

      // The error count button title isn't its description
      if (id !== "command-button-errorcount") {
        is(
          matchedButtons[0].getAttribute("title"),
          tool.description,
          "The tooltip for button matches the tool definition."
        );
      }

      if (TOGGLE_BUTTONS.includes(id)) {
        is(
          matchedButtons[0].getAttribute("aria-pressed"),
          "false",
          `The aria-pressed attribute is set to false for ${id} button`
        );
      } else {
        is(
          matchedButtons[0].getAttribute("aria-pressed"),
          null,
          `The ${id} button does not have the aria-pressed attribute`
        );
      }
    } else {
      is(
        matchedButtons.length,
        0,
        "There should not be a DOM button for the invisible: " + id
      );
    }
  }

  // Store modified pref names so that they can be cleared on error.
  for (const tool of toolbarButtons) {
    const pref = tool.visibilityswitch;
    modifiedPrefs.push(pref);
  }

  // Try checking each checkbox, making sure that it changes the preference
  for (const node of checkNodes) {
    const tool = toolbarButtons.filter(
      commandButton => commandButton.id === node.id
    )[0];
    const isVisible = getBoolPref(tool.visibilityswitch);

    testPreferenceAndUIStateIsConsistent(toolbox, optionsPanelWin);
    node.click();
    testPreferenceAndUIStateIsConsistent(toolbox, optionsPanelWin);

    const isVisibleAfterClick = getBoolPref(tool.visibilityswitch);

    is(
      isVisible,
      !isVisibleAfterClick,
      "Clicking on the node should have toggled visibility preference for " +
        tool.visibilityswitch
    );

    if (isVisibleAfterClick) {
      const matchedButton = toolbox.doc.getElementById(tool.id);
      if (TOGGLE_BUTTONS.includes(tool.id)) {
        is(
          matchedButton.getAttribute("aria-pressed"),
          "false",
          `The aria-pressed attribute is set to false for ${tool.id} button`
        );
      } else {
        is(
          matchedButton.getAttribute("aria-pressed"),
          null,
          `The ${tool.id} button does not have the aria-pressed attribute`
        );
      }
    }
  }
}

async function testPrefsAreRespectedWhenReopeningToolbox() {
  info("Closing toolbox to test after reopening");
  await gDevTools.closeToolboxForTab(gBrowser.selectedTab);

  const toolbox = await gDevTools.showToolboxForTab(gBrowser.selectedTab);
  const optionsPanelWin = await selectOptionsPanel(toolbox);

  info("Toolbox has been reopened.  Checking UI state.");
  await testPreferenceAndUIStateIsConsistent(toolbox, optionsPanelWin);
  return toolbox;
}

function testPreferenceAndUIStateIsConsistent(toolbox, optionsPanelWin) {
  const checkNodes = [
    ...optionsPanelWin.document.querySelectorAll(
      "#enabled-toolbox-buttons-box input[type=checkbox]"
    ),
  ];
  const toolboxButtonNodes = [
    ...toolbox.doc.querySelectorAll(".command-button"),
  ];

  for (const tool of toolbox.toolbarButtons) {
    const isVisible = getBoolPref(tool.visibilityswitch);

    const button = toolboxButtonNodes.find(
      toolboxButton => toolboxButton.id === tool.id
    );
    is(!!button, isVisible, "Button visibility matches pref for " + tool.id);

    const check = checkNodes.filter(node => node.id === tool.id)[0];
    if (check) {
      is(
        check.checked,
        isVisible,
        "Checkbox should be selected based on current pref for " + tool.id
      );
    }
  }
}

async function testButtonStateOnClick(toolbox) {
  const toolboxButtons = ["#command-button-rulers", "#command-button-measure"];
  for (const toolboxButton of toolboxButtons) {
    const button = toolbox.doc.querySelector(toolboxButton);
    if (!button) {
      ok(false, `Couldn't find ${toolboxButton}`);
      continue;
    }

    const isChecked = waitUntil(() => button.classList.contains("checked"));
    is(
      button.getAttribute("aria-pressed"),
      "false",
      `${toolboxButton} has aria-pressed set to false when it's off`
    );

    button.click();
    await isChecked;
    ok(
      button.classList.contains("checked"),
      `Button for ${toolboxButton} can be toggled on`
    );
    is(
      button.getAttribute("aria-pressed"),
      "true",
      `${toolboxButton} has aria-pressed set to true when it's on`
    );

    const isUnchecked = waitUntil(() => !button.classList.contains("checked"));
    button.click();
    await isUnchecked;
    ok(
      !button.classList.contains("checked"),
      `Button for ${toolboxButton} can be toggled off`
    );
    is(
      button.getAttribute("aria-pressed"),
      "false",
      `aria-pressed is set back to false on ${toolboxButton} after it has been toggled off`
    );
  }
}

function getBoolPref(key) {
  try {
    return Services.prefs.getBoolPref(key);
  } catch (e) {
    return false;
  }
}
