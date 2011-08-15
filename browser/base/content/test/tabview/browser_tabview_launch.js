/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let timerId;
let newWin;

// ----------
function test() {
  waitForExplicitFinish();

  // launch tab view for the first time
  newWindowWithTabView(function() {}, function(win) {
    newWin = win;

    let onSelect = function(event) {
      if (deck != event.target)
        return;

      let iframe = win.document.getElementById("tab-view");
      if (deck.selectedPanel != iframe)
        return;

      deck.removeEventListener("select", onSelect, true);

      whenTabViewIsShown(function() {
        executeSoon(function() {
          testMethodToHideAndShowTabView(function() {
            newWin.document.getElementById("menu_tabview").doCommand();
          }, function() {
            testMethodToHideAndShowTabView(function() {
              EventUtils.synthesizeKey("e", { accelKey: true, shiftKey: true }, newWin);
            }, finish);
          });
        });
      }, win);
    };

    let deck = win.document.getElementById("tab-view-deck");
    deck.addEventListener("select", onSelect, true);
  });

  registerCleanupFunction(function () {
    newWin.close();
  });
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
