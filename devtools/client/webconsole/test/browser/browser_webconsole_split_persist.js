/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the split console state is persisted.

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html><p>Web Console test for splitting</p>";

add_task(async function() {
  const getFluentString = await getFluentStringHelper([
    "devtools/client/toolbox.ftl",
  ]);
  const hideLabel = getFluentString("toolbox-meatball-menu-hideconsole-label");
  const showLabel = getFluentString("toolbox-meatball-menu-splitconsole-label");

  info("Opening a tab while there is no user setting on split console pref");
  let toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  ok(!toolbox.splitConsole, "Split console is hidden by default");
  is(
    await getSplitConsoleMenuLabel(toolbox),
    showLabel,
    "Split console menu item says split by default"
  );

  await toggleSplitConsoleWithEscape(toolbox);
  ok(toolbox.splitConsole, "Split console is now visible.");
  is(
    await getSplitConsoleMenuLabel(toolbox),
    hideLabel,
    "Split console menu item now says hide"
  );
  ok(getVisiblePrefValue(), "Visibility pref is true");

  is(
    parseInt(getHeightPrefValue(), 10),
    parseInt(toolbox.webconsolePanel.style.height, 10),
    "Panel height matches the pref"
  );
  toolbox.webconsolePanel.style.height = "200px";

  await toolbox.destroy();

  info(
    "Opening a tab while there is a true user setting on split console pref"
  );
  toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  ok(toolbox.splitConsole, "Split console is visible by default.");
  ok(
    isInputFocused(toolbox.getPanel("webconsole").hud),
    "Split console input is focused by default"
  );
  is(
    await getSplitConsoleMenuLabel(toolbox),
    hideLabel,
    "Split console menu item initially says hide"
  );
  is(
    getHeightPrefValue(),
    200,
    "Height is set based on panel height after closing"
  );

  toolbox.webconsolePanel.style.height = "1px";
  ok(
    toolbox.webconsolePanel.clientHeight > 1,
    "The actual height of the console is bound with a min height"
  );

  toolbox.webconsolePanel.style.height = "10000px";
  ok(
    toolbox.webconsolePanel.clientHeight < 10000,
    "The actual height of the console is bound with a max height"
  );

  await toggleSplitConsoleWithEscape(toolbox);
  ok(!toolbox.splitConsole, "Split console is now hidden.");
  is(
    await getSplitConsoleMenuLabel(toolbox),
    showLabel,
    "Split console menu item now says split"
  );
  ok(!getVisiblePrefValue(), "Visibility pref is false");

  await toolbox.destroy();

  is(
    getHeightPrefValue(),
    10000,
    "Height is set based on panel height after closing"
  );

  info(
    "Opening a tab while there is a false user setting on split " +
      "console pref"
  );
  toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  ok(!toolbox.splitConsole, "Split console is hidden by default.");
  ok(!getVisiblePrefValue(), "Visibility pref is false");

  await toolbox.destroy();
});

function getVisiblePrefValue() {
  return Services.prefs.getBoolPref("devtools.toolbox.splitconsoleEnabled");
}

function getHeightPrefValue() {
  return Services.prefs.getIntPref("devtools.toolbox.splitconsoleHeight");
}

async function getSplitConsoleMenuLabel(toolbox) {
  const button = toolbox.doc.getElementById("toolbox-meatball-menu-button");
  await waitUntil(
    () => toolbox.win.getComputedStyle(button).pointerEvents === "auto"
  );
  return new Promise(resolve => {
    EventUtils.synthesizeMouseAtCenter(button, {}, toolbox.win);

    toolbox.doc.addEventListener(
      "popupshown",
      () => {
        const menuItem = toolbox.doc.getElementById(
          "toolbox-meatball-menu-splitconsole"
        );

        toolbox.doc.addEventListener(
          "popuphidden",
          () => {
            resolve(menuItem?.querySelector(".label")?.textContent);
          },
          { once: true }
        );
        EventUtils.synthesizeKey("KEY_Escape");
      },
      { once: true }
    );
  });
}

function toggleSplitConsoleWithEscape(toolbox) {
  const onceSplitConsole = toolbox.once("split-console");
  const toolboxWindow = toolbox.win;
  toolboxWindow.focus();
  EventUtils.sendKey("ESCAPE", toolboxWindow);
  return onceSplitConsole;
}
