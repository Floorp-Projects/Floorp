/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let callCount = 0;

  newWindow(function (win) {
    registerCleanupFunction(function () win.close());

    win.TabView._initFrame(function () {
      is(callCount++, 0, "call count is zero");
      ok(win.TabView.getContentWindow().UI, "content window is loaded");
    });

    win.TabView._initFrame(function () {
      is(callCount++, 1, "call count is one");
      ok(win.TabView.getContentWindow().UI, "content window is loaded");
    });

    win.TabView._initFrame(function () {
      is(callCount, 2, "call count is two");
      ok(win.TabView.getContentWindow().UI, "content window is loaded");
      finish();
    });
  });
}

function newWindow(callback) {
  let opts = "chrome,all,dialog=no,height=800,width=800";
  let win = window.openDialog(getBrowserURL(), "_blank", opts, "about:blank");
  whenDelayedStartupFinished(win, () => callback(win));
}
