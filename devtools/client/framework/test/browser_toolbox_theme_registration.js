/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from shared-head.js */
"use strict";

// Test for dynamically registering and unregistering themes
const CHROME_URL = "chrome://mochitests/content/browser/devtools/client/framework/test/";

var toolbox;

add_task(function* themeRegistration() {
  let tab = yield addTab("data:text/html,test");
  let target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target, "options");

  let themeId = yield new Promise(resolve => {
    gDevTools.once("theme-registered", (e, registeredThemeId) => {
      resolve(registeredThemeId);
    });

    gDevTools.registerTheme({
      id: "test-theme",
      label: "Test theme",
      stylesheets: [CHROME_URL + "doc_theme.css"],
      classList: ["theme-test"],
    });
  });

  is(themeId, "test-theme", "theme-registered event handler sent theme id");

  ok(gDevTools.getThemeDefinitionMap().has(themeId), "theme added to map");
});

add_task(function* themeInOptionsPanel() {
  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let themeBox = doc.getElementById("devtools-theme-box");
  let testThemeOption = themeBox.querySelector(
    "input[type=radio][value=test-theme]");

  ok(testThemeOption, "new theme exists in the Options panel");

  let lightThemeOption = themeBox.querySelector(
    "input[type=radio][value=light]");

  let color = panelWin.getComputedStyle(themeBox).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  let onThemeSwithComplete = once(panelWin, "theme-switch-complete");

  // Select test theme.
  testThemeOption.click();

  info("Waiting for theme to finish loading");
  yield onThemeSwithComplete;

  color = panelWin.getComputedStyle(themeBox).color;
  is(color, "rgb(255, 0, 0)", "style applied");

  onThemeSwithComplete = once(panelWin, "theme-switch-complete");

  // Select light theme
  lightThemeOption.click();

  info("Waiting for theme to finish loading");
  yield onThemeSwithComplete;

  color = panelWin.getComputedStyle(themeBox).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  onThemeSwithComplete = once(panelWin, "theme-switch-complete");
  // Select test theme again.
  testThemeOption.click();
  yield onThemeSwithComplete;
});

add_task(function* themeUnregistration() {
  let onUnRegisteredTheme = once(gDevTools, "theme-unregistered");
  gDevTools.unregisterTheme("test-theme");
  yield onUnRegisteredTheme;

  ok(!gDevTools.getThemeDefinitionMap().has("test-theme"),
    "theme removed from map");

  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let themeBox = doc.getElementById("devtools-theme-box");

  // The default light theme must be selected now.
  is(themeBox.querySelector("#devtools-theme-box [value=light]").checked, true,
    "light theme must be selected");
});

add_task(function* cleanup() {
  yield toolbox.destroy();
  toolbox = null;
});
