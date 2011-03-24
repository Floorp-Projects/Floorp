// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

Components.utils.import("resource://gre/modules/Services.jsm");

function test() {
  waitForExplicitFinish();

  let win = Services.ww.openWindow(
    window, "chrome://mochitests/content/browser/dom/tests/browser/test_643083.xul", "_blank", "chrome", {});
  win.addEventListener("load", function() {
    let browser = win.browser();
    browser.messageManager.addMessageListener("scroll", function fn(msg) {
      // Calling win.close() will dispatch the close later if the
      // JS context of the window is the same as for the win.close()
      // call. (See nsGlobalWindow::FinalClose())
      //
      // Because the crash needs to happen during the same event that
      // we receive the message, we use an event listener and dispatch
      // to it synchronously.
      //
      window.addEventListener("dummy-event", function() {
        win.close();

        setTimeout(function() {
          ok(true, "Completed message to close window");
          finish();
        }, 0);
      }, false);

      let e = document.createEvent("UIEvents");
      e.initUIEvent("dummy-event", true, false, window, 0);
      window.dispatchEvent(e);

      finish();
    });
  }, false);
}
