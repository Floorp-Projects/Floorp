/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newWin;

// ----------
function test() {
  waitForExplicitFinish();

  let panelSelected = false;
  registerCleanupFunction(function () ok(panelSelected, "panel is selected"));

  let onLoad = function (win) {
    registerCleanupFunction(function () win.close());

    newWin = win;

    let onSelect = function(event) {
      if (deck != event.target)
        return;

      let iframe = win.document.getElementById("tab-view");
      if (deck.selectedPanel != iframe)
        return;

      deck.removeEventListener("select", onSelect, true);
      panelSelected = true;
    };

    let deck = win.document.getElementById("tab-view-deck");
    deck.addEventListener("select", onSelect, true);
  };

  let onShow = function (win) {
    executeSoon(function() {
      testMethodToHideAndShowTabView(function() {
        newWin.document.getElementById("menu_tabview").doCommand();
      }, function() {
        testMethodToHideAndShowTabView(function() {
          EventUtils.synthesizeKey("E", { accelKey: true, shiftKey: true }, newWin);
        }, finish);
      });
    });
  };

  newWindowWithTabView(onShow, onLoad);
}

function testMethodToHideAndShowTabView(executeFunc, callback) {
  whenTabViewIsHidden(function() {
    ok(!newWin.TabView.isVisible(), "Tab View is not visible after executing the function");
    whenTabViewIsShown(function() {
      ok(newWin.TabView.isVisible(), "Tab View is visible after executing the function again");
      callback();
    }, newWin);
    executeFunc();
  }, newWin);
  executeFunc();
}
