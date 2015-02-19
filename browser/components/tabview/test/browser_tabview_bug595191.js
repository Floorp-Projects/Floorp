/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // show the tab view
  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(!TabView.isVisible(), "Tab View is hidden");
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let searchButton = contentWindow.document.getElementById("searchbutton");

  ok(searchButton, "Search button exists");
  
  let onSearchEnabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchenabled", onSearchEnabled, false);
    let search = contentWindow.document.getElementById("search");
    ok(search.style.display != "none", "Search is enabled");
    escapeTest(contentWindow);
  }
  contentWindow.addEventListener("tabviewsearchenabled", onSearchEnabled, 
    false);
  // enter search mode
  EventUtils.sendMouseEvent({ type: "mousedown" }, searchButton, 
    contentWindow);
}

function escapeTest(contentWindow) {  
  let onSearchDisabled = function() {
    contentWindow.removeEventListener(
      "tabviewsearchdisabled", onSearchDisabled, false);

    let search = contentWindow.document.getElementById("search");
    ok(search.style.display == "none", "Search is disabled");
    toggleTabViewTest(contentWindow);
  }
  contentWindow.addEventListener("tabviewsearchdisabled", onSearchDisabled, 
    false);
  EventUtils.synthesizeKey("KEY_Escape", { code: "Escape" }, contentWindow);
}

function toggleTabViewTest(contentWindow) {
  let onTabViewHidden = function() {
    contentWindow.removeEventListener("tabviewhidden", onTabViewHidden, false);

    ok(!TabView.isVisible(), "Tab View is hidden");
    finish();
  }
  contentWindow.addEventListener("tabviewhidden", onTabViewHidden, false);
  // Use keyboard shortcut to toggle back to browser view
  EventUtils.synthesizeKey("e", { accelKey: true, shiftKey: true,
                                  code: "KeyE", keyCode: KeyboardEvent.DOM_VK_E }, contentWindow);
}
