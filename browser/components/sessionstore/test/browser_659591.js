/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let eventReceived = false;

  registerCleanupFunction(function () {
    ok(eventReceived, "SSWindowClosing event received");
  });

  newWindow(function (win) {
    win.addEventListener("SSWindowClosing", function onWindowClosing() {
      win.removeEventListener("SSWindowClosing", onWindowClosing, false);
      eventReceived = true;
      waitForFocus(finish);
    }, false);

    win.close();
  });
}

function newWindow(callback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(() => callback(win));
  }, false);
}
