/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(onTabViewShown, null, 850);
}

function onTabViewShown(win) {
  registerCleanupFunction(function () win.close());

  let contentWindow = win.TabView.getContentWindow();
  let currentGroup = contentWindow.GroupItems.getActiveGroupItem();

  function checkResized(diffX, diffY, shouldResize, text, callback) {
    let {width: origWidth, height: origHeight} = currentGroup.getBounds();

    resizeWindow(win, diffX, diffY, function () {
      let {width: newWidth, height: newHeight} = currentGroup.getBounds();
      let resized = (origWidth != newWidth || origHeight != newHeight);

      is(resized, shouldResize, text + ": The group should " +
         (shouldResize ? "" : "not ") + "have been resized");

      callback();
    });
  }

  function next() {
    let test = tests.shift();

    if (test)
      checkResized.apply(this, test.concat([next]));
    else
      finishTest();
  }

  function finishTest() {
    // reset the usersize of the group, so this should clear the "cramped" feeling.
    currentGroup.setSize(100, 100, true);
    currentGroup.setUserSize();
    checkResized(400, 400, false, "After clearing the cramp", finish);
  }

  let tests = [
    // diffX, diffY, shouldResize, text
    [ -50,  -50, false, "A little smaller"],
    [  50,   50, false, "A little bigger"],
    [-400, -400, true,  "Much smaller"],
    [ 400,  400, true,  "Bigger after much smaller"],
    [-400, -400, true,  "Much smaller"]
  ];

  // setup
  currentGroup.setSize(600, 600, true);
  currentGroup.setUserSize();

  // run the tests
  next();
}

// ----------
function resizeWindow(win, diffX, diffY, callback) {
  let targetWidth = win.outerWidth + diffX;
  let targetHeight = win.outerHeight + diffY;

  (function tryResize() {
    let {outerWidth: width, outerHeight: height} = win;
    if (width != targetWidth || height != targetHeight) {
      win.resizeTo(targetWidth, targetHeight);
      executeSoon(tryResize);
    } else {
      callback();
    }
  })();
}
