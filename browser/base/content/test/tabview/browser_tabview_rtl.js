/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tabViewShownCount = 0;

// ----------
function test() {
  waitForExplicitFinish();

  // verify initial state
  ok(!TabView.isVisible(), "Tab View starts hidden");

  // use the Tab View button to launch it for the first time
  window.addEventListener("tabviewshown", onTabViewLoadedAndShown("ltr"), false);
  toggleTabView();
}

function toggleTabView() {
  let tabViewCommand = document.getElementById("Browser:ToggleTabView");
  tabViewCommand.doCommand();
}

// ----------
function onTabViewLoadedAndShown(dir) {
  return function() {
    window.removeEventListener("tabviewshown", arguments.callee, false);
    ok(TabView.isVisible(), "Tab View is visible.");

    let contentWindow = document.getElementById("tab-view").contentWindow;
    let contentDocument = contentWindow.document;
    is(contentDocument.documentElement.getAttribute("dir"), dir,
       "The direction should be set to " + dir.toUpperCase());

    // kick off the series
    window.addEventListener("tabviewhidden", onTabViewHidden(dir), false);
    TabView.toggle();
  };
}

// ---------- 
function onTabViewHidden(dir) {
  return function() {
    window.removeEventListener("tabviewhidden", arguments.callee, false);
    ok(!TabView.isVisible(), "Tab View is hidden.");

    if (dir == "ltr") {
      // Switch to RTL mode
      Services.prefs.setCharPref("intl.uidirection.en-US", "rtl");

      // use the Tab View button to launch it for the second time
      window.addEventListener("tabviewshown", onTabViewLoadedAndShown("rtl"), false);
      toggleTabView();
    } else {
      // Switch to LTR mode
      Services.prefs.clearUserPref("intl.uidirection.en-US");

      finish();
    }
  };
}

