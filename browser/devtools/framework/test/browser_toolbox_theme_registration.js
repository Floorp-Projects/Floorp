/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const CHROME_URL = "chrome://mochitests/content/browser/browser/devtools/framework/test/";

var toolbox;

function test()
{
  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(testRegister);
  }, true);

  content.location = "data:text/html,test for dynamically registering and unregistering themes";
}

function testRegister(aToolbox)
{
  toolbox = aToolbox
  gDevTools.once("theme-registered", themeRegistered);

  gDevTools.registerTheme({
    id: "test-theme",
    label: "Test theme",
    stylesheets: [CHROME_URL + "doc_theme.css"],
    classList: ["theme-test"],
  });
}

function themeRegistered(event, themeId)
{
  is(themeId, "test-theme", "theme-registered event handler sent theme id");

  ok(gDevTools.getThemeDefinitionMap().has(themeId), "theme added to map");

  // Test that new theme appears in the Options panel
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.showToolbox(target, "options").then(() => {
    let panel = toolbox.getCurrentPanel();
    let doc = panel.panelWin.frameElement.contentDocument;
    let themeOption = doc.querySelector("#devtools-theme-box > radio[value=test-theme]");

    ok(themeOption, "new theme exists in the Options panel");

    // Apply the new theme.
    applyTheme();
  });
}

function applyTheme()
{
  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let testThemeOption = doc.querySelector("#devtools-theme-box > radio[value=test-theme]");
  let lightThemeOption = doc.querySelector("#devtools-theme-box > radio[value=light]");

  let color = panelWin.getComputedStyle(testThemeOption).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  // Select test theme.
  testThemeOption.click();

  color = panelWin.getComputedStyle(testThemeOption).color;
  is(color, "rgb(255, 0, 0)", "style applied");

  // Select light theme
  lightThemeOption.click();

  color = panelWin.getComputedStyle(testThemeOption).color;
  isnot(color, "rgb(255, 0, 0)", "style unapplied");

  // Select test theme again.
  testThemeOption.click();

  // Then unregister the test theme.
  testUnregister();
}

function testUnregister()
{
  gDevTools.unregisterTheme("test-theme");

  ok(!gDevTools.getThemeDefinitionMap().has("test-theme"), "theme removed from map");

  let panelWin = toolbox.getCurrentPanel().panelWin;
  let doc = panelWin.frameElement.contentDocument;
  let themeBox = doc.querySelector("#devtools-theme-box");

  // The default light theme must be selected now.
  is(themeBox.selectedItem, themeBox.querySelector("[value=light]"),
    "theme light must be selected");

  // Make sure the tab-attaching process is done before we destroy the toolbox.
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let actor = target.activeTab.actor;
  target.client.attachTab(actor, (response) => {
    cleanup();
  });
}

function cleanup()
{
  toolbox.destroy().then(function() {
    toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  });
}
