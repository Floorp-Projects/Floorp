/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("tabviewshown", onTabViewWindowLoaded, false);
  TabView.toggle();
}

function onTabViewWindowLoaded() {
  window.removeEventListener("tabviewshown", onTabViewWindowLoaded, false);

  let contentWindow = document.getElementById("tab-view").contentWindow;
  let number = -1;

  let onSearchEnabled = function() {
    // ensure the dom changes (textbox get focused with number entered) complete 
    // before doing a check.
    executeSoon(function() { 
      let searchBox = contentWindow.document.getElementById("searchbox");
      is(searchBox.value, number, "The seach box matches the number: " + number);
      contentWindow.Search.hide(null);
    });
  }
  let onSearchDisabled = function() {
    if (++number <= 9) {
      EventUtils.synthesizeKey(String(number), { }, contentWindow);
    } else {
      contentWindow.removeEventListener(
        "tabviewsearchenabled", onSearchEnabled, false);
      contentWindow.removeEventListener(
        "tabviewsearchdisabled", onSearchDisabled, false);

      let endGame = function() {
        window.removeEventListener("tabviewhidden", endGame, false);

        ok(!TabView.isVisible(), "Tab View is hidden");
        finish();
      }
      window.addEventListener("tabviewhidden", endGame, false);
      TabView.toggle();
    }
  }
  contentWindow.addEventListener(
    "tabviewsearchenabled", onSearchEnabled, false);
  contentWindow.addEventListener(
    "tabviewsearchdisabled", onSearchDisabled, false);

  onSearchDisabled();
}

