/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test for dynamically registering and unregistering themes
const CHROME_URL = "chrome://mochitests/content/browser/devtools/client/framework/test/";

var toolbox;

add_task(function* themeRegistration() {
  let tab = yield addTab("data:text/html,test");
  let target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target);

  let themeId = yield new Promise(resolve => {
    gDevTools.once("theme-registered", (e, themeId) => {
      resolve(themeId);
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

  yield toolbox.selectTool("options");

  let panel = toolbox.getCurrentPanel();
  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let themeOption = doc.querySelector("#devtools-theme-box > radio[value=test-theme]");

  ok(themeOption, "new theme exists in the Options panel");

  let testThemeOption = doc.querySelector("#devtools-theme-box > radio[value=test-theme]");
  let lightThemeOption = doc.querySelector("#devtools-theme-box > radio[value=light]");

  let color = panelWin.getComputedStyle(testThemeOption).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  // Select test theme.
  testThemeOption.click();

  info("Waiting for theme to finish loading");
  yield once(panelWin, "theme-switch-complete");

  color = panelWin.getComputedStyle(testThemeOption).color;
  is(color, "rgb(255, 0, 0)", "style applied");

  // Select light theme
  lightThemeOption.click();

  info("Waiting for theme to finish loading");
  yield once(panelWin, "theme-switch-complete");

  color = panelWin.getComputedStyle(testThemeOption).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  // Select test theme again.
  testThemeOption.click();
});

add_task(function* themeUnregistration() {
  gDevTools.unregisterTheme("test-theme");

  ok(!gDevTools.getThemeDefinitionMap().has("test-theme"), "theme removed from map");

  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let themeBox = doc.querySelector("#devtools-theme-box");

  // The default light theme must be selected now.
  is(themeBox.selectedItem, themeBox.querySelector("[value=light]"),
    "theme light must be selected");
});

add_task(function* cleanup() {
  yield toolbox.destroy();
  toolbox = null;
});
