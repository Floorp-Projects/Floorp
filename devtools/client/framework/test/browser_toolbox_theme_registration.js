/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for dynamically registering and unregistering themes
const CHROME_URL = "chrome://mochitests/content/browser/devtools/client/framework/test/";
const TEST_THEME_NAME = "test-theme";
const LIGHT_THEME_NAME = "light";

var toolbox;

add_task(async function themeRegistration() {
  const tab = await addTab("data:text/html,test");
  const target = TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target, "options");

  const themeId = await new Promise(resolve => {
    gDevTools.once("theme-registered", registeredThemeId => {
      resolve(registeredThemeId);
    });

    gDevTools.registerTheme({
      id: TEST_THEME_NAME,
      label: "Test theme",
      stylesheets: [CHROME_URL + "doc_theme.css"],
      classList: ["theme-test"],
    });
  });

  is(themeId, TEST_THEME_NAME, "theme-registered event handler sent theme id");

  ok(gDevTools.getThemeDefinitionMap().has(themeId), "theme added to map");
});

add_task(async function themeInOptionsPanel() {
  const panelWin = toolbox.getCurrentPanel().panelWin;
  const doc = panelWin.frameElement.contentDocument;
  const themeBox = doc.getElementById("devtools-theme-box");
  const testThemeOption = themeBox.querySelector(
    `input[type=radio][value=${TEST_THEME_NAME}]`);
  const eventsRecorded = [];

  function onThemeChanged(theme) {
    eventsRecorded.push(theme);
  }
  gDevTools.on("theme-changed", onThemeChanged);

  ok(testThemeOption, "new theme exists in the Options panel");

  const lightThemeOption = themeBox.querySelector(
    `input[type=radio][value=${LIGHT_THEME_NAME}]`);

  let color = panelWin.getComputedStyle(themeBox).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  let onThemeSwithComplete = once(panelWin, "theme-switch-complete");

  // Select test theme.
  testThemeOption.click();

  info("Waiting for theme to finish loading");
  await onThemeSwithComplete;

  is(gDevTools.getTheme(), TEST_THEME_NAME, "getTheme returns the expected theme");
  is(eventsRecorded.pop(), TEST_THEME_NAME, "theme-changed fired with the expected theme");

  color = panelWin.getComputedStyle(themeBox).color;
  is(color, "rgb(255, 0, 0)", "style applied");

  onThemeSwithComplete = once(panelWin, "theme-switch-complete");

  // Select light theme
  lightThemeOption.click();

  info("Waiting for theme to finish loading");
  await onThemeSwithComplete;

  is(gDevTools.getTheme(), LIGHT_THEME_NAME, "getTheme returns the expected theme");
  is(eventsRecorded.pop(), LIGHT_THEME_NAME, "theme-changed fired with the expected theme");

  color = panelWin.getComputedStyle(themeBox).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  onThemeSwithComplete = once(panelWin, "theme-switch-complete");
  // Select test theme again.
  testThemeOption.click();
  await onThemeSwithComplete;
  is(gDevTools.getTheme(), TEST_THEME_NAME, "getTheme returns the expected theme");
  is(eventsRecorded.pop(), TEST_THEME_NAME, "theme-changed fired with the expected theme");

  gDevTools.off("theme-changed", onThemeChanged);
});

add_task(async function themeUnregistration() {
  const panelWin = toolbox.getCurrentPanel().panelWin;
  const onUnRegisteredTheme = once(gDevTools, "theme-unregistered");
  const onThemeSwitchComplete = once(panelWin, "theme-switch-complete");
  const eventsRecorded = [];

  function onThemeChanged(theme) {
    eventsRecorded.push(theme);
  }
  gDevTools.on("theme-changed", onThemeChanged);

  gDevTools.unregisterTheme(TEST_THEME_NAME);
  await onUnRegisteredTheme;
  await onThemeSwitchComplete;

  is(gDevTools.getTheme(), LIGHT_THEME_NAME, "getTheme returns the expected theme");
  is(eventsRecorded.pop(), LIGHT_THEME_NAME, "theme-changed fired with the expected theme");
  ok(!gDevTools.getThemeDefinitionMap().has(TEST_THEME_NAME),
    "theme removed from map");

  const doc = panelWin.frameElement.contentDocument;
  const themeBox = doc.getElementById("devtools-theme-box");

  // The default light theme must be selected now.
  is(themeBox.querySelector(`#devtools-theme-box [value=${LIGHT_THEME_NAME}]`).checked, true,
    `${LIGHT_THEME_NAME} theme must be selected`);

  gDevTools.off("theme-changed", onThemeChanged);
});

add_task(async function cleanup() {
  await toolbox.destroy();
  toolbox = null;
});
