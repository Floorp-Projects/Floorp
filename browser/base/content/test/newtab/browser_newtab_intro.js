/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INTRO_PREF = "browser.newtabpage.introShown";
const UPDATE_INTRO_PREF = "browser.newtabpage.updateIntroShown";
const PRELOAD_PREF = "browser.newtab.preload";

function runTests() {
  let origIntro = Services.prefs.getBoolPref(INTRO_PREF);
  let origUpdateIntro = Services.prefs.getBoolPref(UPDATE_INTRO_PREF);
  let origPreload = Services.prefs.getBoolPref(PRELOAD_PREF);
  registerCleanupFunction(_ => {
    Services.prefs.setBoolPref(INTRO_PREF, origIntro);
    Services.prefs.setBoolPref(INTRO_PREF, origUpdateIntro);
    Services.prefs.setBoolPref(PRELOAD_PREF, origPreload);
  });

  // Test with preload false
  Services.prefs.setBoolPref(INTRO_PREF, false);
  Services.prefs.setBoolPref(UPDATE_INTRO_PREF, false);
  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  let intro;
  let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties");
  let brandShortName = brand.GetStringFromName("brandShortName");

  yield addNewTabPageTab();
  intro = getContentDocument().getElementById("newtab-intro-mask");
  is(intro.style.opacity, 1, "intro automatically shown on first opening");
  is(getContentDocument().getElementById("newtab-intro-header").innerHTML,
     'Welcome to New Tab on <span xmlns="http://www.w3.org/1999/xhtml" class="bold">' + brandShortName + '</span>!', "we show the first-run intro.");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");
  is(Services.prefs.getBoolPref(UPDATE_INTRO_PREF), true, "newtab avoids showing update if intro was shown");

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
     'Welcome to New Tab on <span xmlns="http://www.w3.org/1999/xhtml" class="bold">' + brandShortName + '</span>!', "we show the first-run intro.");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "newtab remembers that the intro was shown");
  is(Services.prefs.getBoolPref(UPDATE_INTRO_PREF), true, "newtab avoids showing update if intro was shown");

  // Test with first run true but update false
  Services.prefs.setBoolPref(UPDATE_INTRO_PREF, false);

  yield addNewTabPageTab();
  intro = getContentDocument().getElementById("newtab-intro-mask");
  is(intro.style.opacity, 1, "intro automatically shown on preloaded opening");
  is(getContentDocument().getElementById("newtab-intro-header").innerHTML,
     "New Tab got an update!", "we show the update intro.");
  is(Services.prefs.getBoolPref(INTRO_PREF), true, "INTRO_PREF stays true");
  is(Services.prefs.getBoolPref(UPDATE_INTRO_PREF), true, "newtab remembers that the update intro was show");

  // Test clicking the 'next' and 'back' buttons.
  let buttons = getContentDocument().getElementById("newtab-intro-buttons").getElementsByTagName("input");
  let progress = getContentDocument().getElementById("newtab-intro-numerical-progress");
  let back = buttons[0];
  let next = buttons[1];

  is(progress.getAttribute("page"), 0, "we are on the first page");
  is(intro.style.opacity, 1, "intro visible");


  let createMutationObserver = function(fcn) {
    return new Promise(resolve => {
      let observer = new MutationObserver(function(mutations) {
        fcn();
        observer.disconnect();
        resolve();
      });
      let config = { attributes: true, attributeFilter: ["style"], childList: true };
      observer.observe(progress, config);
    });
  }

  let p = createMutationObserver(function() {
    is(progress.getAttribute("page"), 1, "we get to the 2nd page");
    is(intro.style.opacity, 1, "intro visible");
  });
  next.click();
  yield p.then(TestRunner.next);

  p = createMutationObserver(function() {
    is(progress.getAttribute("page"), 2, "we get to the 3rd page");
    is(intro.style.opacity, 1, "intro visible");
  });
  next.click();
  yield p.then(TestRunner.next);

  p = createMutationObserver(function() {
    is(progress.getAttribute("page"), 1, "go back to 2nd page");
    is(intro.style.opacity, 1, "intro visible");
  });
  back.click();
  yield p.then(TestRunner.next);

  p = createMutationObserver(function() {
    is(progress.getAttribute("page"), 0, "go back to 1st page");
    is(intro.style.opacity, 1, "intro visible");
  });
  back.click();
  yield p.then(TestRunner.next);


  p = createMutationObserver(function() {
    is(progress.getAttribute("page"), 0, "another back will 'skip tutorial'");
    is(intro.style.opacity, 0, "intro exited");
  });
  back.click();
  p.then(TestRunner.next);
}
