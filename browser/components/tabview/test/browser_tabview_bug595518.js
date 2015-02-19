/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  
  // show tab view
  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);
  ok(TabView.isVisible(), "Tab View is visible");

  let contentWindow = document.getElementById("tab-view").contentWindow;

  let onTabViewHidden = function() {
    window.removeEventListener("tabviewhidden", onTabViewHidden, false);

    // verify exit button worked
    ok(!TabView.isVisible(), "Tab View is hidden");
    
    // verify that the exit button no longer has focus
    is(contentWindow.iQ("#exit-button:focus").length, 0, 
       "The exit button doesn't have the focus");

    // verify that the keyboard combo works (this is the crux of bug 595518)
    // Prepare the key combo
    window.addEventListener("tabviewshown", onTabViewShown, false);
    EventUtils.synthesizeKey("e", { accelKey: true, shiftKey: true, code: "KeyE", keyCode: KeyboardEvent.DOM_VK_E }, contentWindow);
  }
  
  let onTabViewShown = function() {
    window.removeEventListener("tabviewshown", onTabViewShown, false);
    
    // test if the key combo worked
    ok(TabView.isVisible(), "Tab View is visible");

    // clean up
    let endGame = function() {
      window.removeEventListener("tabviewhidden", endGame, false);

      ok(!TabView.isVisible(), "Tab View is hidden");
      finish();
    }
    window.addEventListener("tabviewhidden", endGame, false);
    TabView.toggle();
  }

  window.addEventListener("tabviewhidden", onTabViewHidden, false);

  // locate exit button
  let button = contentWindow.document.getElementById("exit-button");
  ok(button, "Exit button exists");

  // click exit button
  button.focus();
  EventUtils.sendMouseEvent({ type: "click" }, button, contentWindow);
}
