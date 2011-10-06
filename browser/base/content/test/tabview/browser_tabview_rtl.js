/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// ----------
function test() {
  waitForExplicitFinish();

  // verify initial state
  ok(!TabView.isVisible(), "Tab View starts hidden");

  showTabView(onTabViewLoadedAndShown("ltr"));
}

// ----------
function onTabViewLoadedAndShown(dir) {
  return function() {
    ok(TabView.isVisible(), "Tab View is visible.");

    let contentWindow = document.getElementById("tab-view").contentWindow;
    let contentDocument = contentWindow.document;
    is(contentDocument.documentElement.getAttribute("dir"), dir,
       "The direction should be set to " + dir.toUpperCase());

    // kick off the series
    hideTabView(onTabViewHidden(dir));
  };
}

// ---------- 
function onTabViewHidden(dir) {
  return function() {
    ok(!TabView.isVisible(), "Tab View is hidden.");

    if (dir == "ltr") {
      // Switch to RTL mode
      Services.prefs.setCharPref("intl.uidirection.en-US", "rtl");

      showTabView(onTabViewLoadedAndShown("rtl"));
    } else {
      // Switch to LTR mode
      Services.prefs.clearUserPref("intl.uidirection.en-US");

      finish();
    }
  };
}
