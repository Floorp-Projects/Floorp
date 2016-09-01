/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests ensure that all changes made to the new tab page in private
 * browsing mode are discarded after switching back to normal mode again.
 * The private browsing mode should start with the current grid shown in normal
 * mode.
 */

add_task(function* () {
  // prepare the grid
  yield testOnWindow(undefined);
  yield setLinks("0,1,2,3,4,5,6,7,8,9");

  yield* addNewTabPageTab();
  yield pinCell(0);
  yield* checkGrid("0p,1,2,3,4,5,6,7,8");

  // open private window
  yield testOnWindow({private: true});

  yield* addNewTabPageTab();
  yield* checkGrid("0p,1,2,3,4,5,6,7,8");

  // modify the grid while we're in pb mode
  yield blockCell(1);
  yield* checkGrid("0p,2,3,4,5,6,7,8");

  yield unpinCell(0);
  yield* checkGrid("0,2,3,4,5,6,7,8");

  // open normal window
  yield testOnWindow(undefined);

  // check that the grid is the same as before entering pb mode
  yield* addNewTabPageTab();
  yield* checkGrid("0,2,3,4,5,6,7,8")
});

var windowsToClose = [];
function* testOnWindow(options) {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  var win = OpenBrowserWindow(options);
  windowsToClose.push(win);
  gWindow = win;
  yield newWindowPromise;
}

registerCleanupFunction(function () {
  gWindow = window;
  windowsToClose.forEach(function(win) {
    win.close();
  });
});

