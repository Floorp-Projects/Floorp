/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests ensure that all changes made to the new tab page in private
 * browsing mode are discarded after switching back to normal mode again.
 * The private browsing mode should start with the current grid shown in normal
 * mode.
 */

add_task(async function() {
  // prepare the grid
  await testOnWindow(undefined);
  await setLinks("0,1,2,3,4,5,6,7,8,9");

  await addNewTabPageTab();
  await pinCell(0);
  await checkGrid("0p,1,2,3,4,5,6,7,8");

  // open private window
  await testOnWindow({private: true});

  await addNewTabPageTab();
  await checkGrid("0p,1,2,3,4,5,6,7,8");

  // modify the grid while we're in pb mode
  await blockCell(1);
  await checkGrid("0p,2,3,4,5,6,7,8");

  await unpinCell(0);
  await checkGrid("0,2,3,4,5,6,7,8");

  // open normal window
  await testOnWindow(undefined);

  // check that the grid is the same as before entering pb mode
  await addNewTabPageTab();
  await checkGrid("0,2,3,4,5,6,7,8");
});

var windowsToClose = [];
async function testOnWindow(options) {
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  var win = OpenBrowserWindow(options);
  windowsToClose.push(win);
  gWindow = win;
  await newWindowPromise;
}

registerCleanupFunction(function() {
  gWindow = window;
  windowsToClose.forEach(function(win) {
    win.close();
  });
});

