const SAVE_PER_SITE_PREF = "browser.download.lastDir.savePerSite";

function test_deleted_iframe(perSitePref, windowOptions={}) {
  return function*() {
    Services.prefs.setBoolPref(SAVE_PER_SITE_PREF, perSitePref);
    let {DownloadLastDir} = Cu.import("resource://gre/modules/DownloadLastDir.jsm", {});

    let win = yield promiseOpenAndLoadWindow(windowOptions);
    let tab = win.gBrowser.addTab();
    yield promiseTabLoadEvent(tab, "about:mozilla");

    let doc = tab.linkedBrowser.contentDocument;
    let iframe = doc.createElement("iframe");
    doc.body.appendChild(iframe);

    ok(iframe.contentWindow, "iframe should have a window");
    let gDownloadLastDir = new DownloadLastDir(iframe.contentWindow);
    let cw = iframe.contentWindow;
    let promiseIframeWindowGone = new Promise((resolve, reject) => {
      Services.obs.addObserver(function obs(subject, topic) {
        if (subject == cw) {
          Services.obs.removeObserver(obs, topic);
          resolve();
        }
      }, "dom-window-destroyed", false);
    });
    iframe.remove();
    yield promiseIframeWindowGone;
    cw = null;
    ok(!iframe.contentWindow, "Managed to destroy iframe");

    let someDir = "blah";
    try {
      someDir = yield new Promise((resolve, reject) => {
        gDownloadLastDir.getFileAsync("http://www.mozilla.org/", function(dir) {
          resolve(dir);
        });
      });
    } catch (ex) {
      ok(false, "Got an exception trying to get the directory where things should be saved.");
      Cu.reportError(ex);
    }
    // NB: someDir can legitimately be null here when set, hence the 'blah' workaround:
    isnot(someDir, "blah", "Should get a file even after the window was destroyed.");

    try {
      gDownloadLastDir.setFile("http://www.mozilla.org/", null);
    } catch (ex) {
      ok(false, "Got an exception trying to set the directory where things should be saved.");
      Cu.reportError(ex);
    }

    yield promiseWindowClosed(win);
    Services.prefs.clearUserPref(SAVE_PER_SITE_PREF);
  };
}

add_task(test_deleted_iframe(false));
add_task(test_deleted_iframe(false));
add_task(test_deleted_iframe(true, {private: true}));
add_task(test_deleted_iframe(true, {private: true}));

