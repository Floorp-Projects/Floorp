/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function animateZoom() prefsBranch.getBoolPref("animate_zoom");

let contentWindow = null;

registerCleanupFunction(function() {
  // reset to default: true
  prefsBranch.setBoolPref("animate_zoom", true);
});

function test() {
  waitForExplicitFinish();
  
  ok(!TabView.isVisible(), "Tab View is not visible");
  
  window.addEventListener("tabviewshown", startTesting, false);
  TabView.toggle();
}

function startTesting() {
  window.removeEventListener("tabviewshown", startTesting, false);
  
  contentWindow = document.getElementById("tab-view").contentWindow;
  
  ok(TabView.isVisible(), "Tab View is visible");
  
  ok(animateZoom(), "By default, we want to animate");
  
  function finishOnHidden() {
    contentWindow.removeEventListener("tabviewhidden", finishOnHidden, false);
    ok(!TabView.isVisible(), "Tab View is not visible");
    finish();
  }
  
  function wrapup() {
    contentWindow.addEventListener("tabviewhidden", finishOnHidden, false);
    TabView.hide();
  }

  function part2() {
    prefsBranch.setBoolPref("animate_zoom", false);
    ok(!animateZoom(), "animate_zoom = false");
    zoomInAndOut(false, wrapup);
  }

  function part1() {
    prefsBranch.setBoolPref("animate_zoom",true);
    ok(animateZoom(), "animate_zoom = true");
    zoomInAndOut(true, part2);
  }
    
  // start it up!
  part1();
}
  
function zoomInAndOut(transitionsExpected, callback) {
  let transitioned = 0;

  contentWindow.document.addEventListener("transitionend", onTransitionEnd, false);
  function onTransitionEnd(event) {
    transitioned++;
  }

  let onHidden = function() {
    contentWindow.removeEventListener("tabviewhidden", onHidden, false);
    if (transitionsExpected)
      ok(transitioned >= 0, "There can be transitions");
    else
      ok(!transitioned, "There should have been no transitions");
    TabView.toggle();
  };
  
  let onShownAgain = function() {
    contentWindow.removeEventListener("tabviewshown", onShownAgain, false);
    if (transitionsExpected)
      ok(transitioned >= 0, "There can be transitions");
    else
      ok(!transitioned, "There should have been no transitions");

    contentWindow.document.removeEventListener("transitionend", onTransitionEnd, false);
    callback();
  };
  
  contentWindow.addEventListener("tabviewhidden", onHidden, false);
  contentWindow.addEventListener("tabviewshown", onShownAgain, false);
  
  // get this party started by hiding tab view
  TabView.toggle();
}
