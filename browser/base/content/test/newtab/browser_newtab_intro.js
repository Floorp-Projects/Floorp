/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INTRO_PREF = "browser.newtabpage.introShown";
const PRELOAD_PREF = "browser.newtab.preload";

function runTests() {
  let origIntro = Services.prefs.getBoolPref(INTRO_PREF);
  let origPreload = Services.prefs.getBoolPref(PRELOAD_PREF);
  registerCleanupFunction(_ => {
    Services.prefs.setBoolPref(INTRO_PREF, origIntro);
    Services.prefs.setBoolPref(PRELOAD_PREF, origPreload);
  });

  // Test with preload false
  Services.prefs.setBoolPref(INTRO_PREF, false);
  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  let panel;
  function maybeWaitForPanel() {
    // If already open, no need to wait
    if (panel.state == "open") {
      executeSoon(TestRunner.next);
      return;
    }

    // We're expecting the panel to open, so wait for it
    panel.addEventListener("popupshown", TestRunner.next);
    isnot(panel.state, "open", "intro panel can be slow to show");
  }

  yield addNewTabPageTab();
  panel = getContentDocument().getElementById("newtab-intro-panel");
  yield maybeWaitForPanel();
  is(panel.state, "open", "intro automatically shown on first opening");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");

  yield addNewTabPageTab();
  panel = getContentDocument().getElementById("newtab-intro-panel");
  is(panel.state, "closed", "intro not shown on second opening");

  // Test with preload true
  Services.prefs.setBoolPref(INTRO_PREF, false);
  Services.prefs.setBoolPref(PRELOAD_PREF, true);

  yield addNewTabPageTab();
  panel = getContentDocument().getElementById("newtab-intro-panel");
  yield maybeWaitForPanel();
  is(panel.state, "open", "intro automatically shown on preloaded opening");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");
}
