/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function animateZoom() prefsBranch.getBoolPref("animate_zoom");

function test() {
  waitForExplicitFinish();
  
  let charsetArg = "charset=" + window.content.document.characterSet;
  let win = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no",
                              "about:blank", charsetArg, null, null, true);

  registerCleanupFunction(function() {
    prefsBranch.setBoolPref("animate_zoom", true);
    win.close();
  });

  ok(animateZoom(), "By default, we animate on zoom.");
  prefsBranch.setBoolPref("animate_zoom", false);
  ok(!animateZoom(), "animate_zoom = false");
  
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    // a few shared references
    let tabViewWindow = null;
    let transitioned = 0;

    let initCallback = function() {
      tabViewWindow = win.TabView._window;
      function onTransitionEnd(event) {
        transitioned++;
        info(transitioned);
      }
      tabViewWindow.document.addEventListener("transitionend", onTransitionEnd, false);

      showTabView(function() {
        ok(!transitioned, "There should be no transitions");

        tabViewWindow.document.removeEventListener(
          "transitionend", onTransitionEnd, false);

        finish();
      }, win);
    };

    win.TabView._initFrame(initCallback);
  }
  win.addEventListener("load", onLoad, false);
}
