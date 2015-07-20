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

  yield addNewTabPageTab();
  let intro;
  intro = getContentDocument().getElementById("newtab-intro-mask");
  is(intro.style.opacity, 1, "intro automatically shown on first opening");
  is(getContentDocument().getElementById("newtab-intro-header").innerHTML,
     'New Tab got an update!', "we show intro.");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");

  yield addNewTabPageTab();
  intro = getContentDocument().getElementById("newtab-intro-mask");
  is(intro.style.opacity, 0, "intro not shown on second opening");

  // Test with preload true
  Services.prefs.setBoolPref(INTRO_PREF, false);
  Services.prefs.setBoolPref(PRELOAD_PREF, true);

  yield addNewTabPageTab();
  intro = getContentDocument().getElementById("newtab-intro-mask");
  is(intro.style.opacity, 1, "intro automatically shown on preloaded opening");
  is(getContentDocument().getElementById("newtab-intro-header").innerHTML,
     'New Tab got an update!', "we show intro.");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");

  let gotit = getContentDocument().getElementById("newtab-intro-button");
  gotit.click();

  is(intro.style.opacity, 0, "intro exited");
}
