/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://example.com/browser/browser/base/content/test/general/dummy_page.html";

function test() {
  let win;

  let listener = {
    onLocationChange: (webProgress, request, uri, flags) => {
      ok(webProgress.isTopLevel, "Received onLocationChange from top frame");
      is(uri.spec, URL, "Received onLocationChange for correct URL");
      finish();
    }
  };

  waitForExplicitFinish();

  // Remove the listener and window when we're done.
  registerCleanupFunction(() => {
    win.gBrowser.removeProgressListener(listener);
    win.close();
  });

  // Wait for the newly opened window.
  whenNewWindowOpened(w => win = w);

  // Open a link in a new window.
  openLinkIn(URL, "window", {});

  // On the next tick, but before the window has finished loading, access the
  // window's gBrowser property to force the tabbrowser constructor early.
  (function tryAddProgressListener() {
    executeSoon(() => {
      try {
        win.gBrowser.addProgressListener(listener);
      } catch (e) {
        // win.gBrowser wasn't ready, yet. Try again in a tick.
        tryAddProgressListener();
      }
    });
  })();
}

function whenNewWindowOpened(cb) {
  Services.obs.addObserver(function obs(win) {
    Services.obs.removeObserver(obs, "domwindowopened");
    cb(win);
  }, "domwindowopened", false);
}
