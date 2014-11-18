/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that changing preferences in the options panel updates the prefs
// and toggles appropriate things in the toolbox.

let doc = null, toolbox = null, panelWin = null;

const PREF_ENABLED = "browser.devedition.theme.enabled";
const PREF_SHOW = "browser.devedition.theme.showCustomizeButton";

const URL = "data:text/html;charset=utf8,test for toggling dev edition browser theme toggling";

let test = asyncTest(function*() {
  // Set preference to false by default so this could
  // run in Developer Edition which has it on by default.
  Services.prefs.setBoolPref(PREF_ENABLED, false);
  Services.prefs.setBoolPref(PREF_SHOW, true);

  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target);
  let tool = yield toolbox.selectTool("options");
  panelWin = tool.panelWin;

  let checkbox = tool.panelDoc.getElementById("devtools-browser-theme");

  ise(Services.prefs.getBoolPref(PREF_ENABLED), false, "Dev Theme pref off on start");

  let themeStatus = yield clickAndWaitForThemeChange(checkbox, panelWin);
  ise(themeStatus, true, "Theme has been toggled on.");

  themeStatus = yield clickAndWaitForThemeChange(checkbox, panelWin);
  ise(themeStatus, false, "Theme has been toggled off.");

  yield cleanup();
});

function clickAndWaitForThemeChange (el, win) {
  let deferred = promise.defer();
  gDevTools.on("pref-changed", function handler (event, {pref}) {
    if (pref === PREF_ENABLED) {
      gDevTools.off("pref-changed", handler);
      deferred.resolve(Services.prefs.getBoolPref(PREF_ENABLED));
    }
  });

  EventUtils.synthesizeMouseAtCenter(el, {}, win);

  return deferred.promise;
}

function* cleanup() {
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(PREF_ENABLED);
  Services.prefs.clearUserPref(PREF_SHOW);
  toolbox = doc = panelWin = null;
}
