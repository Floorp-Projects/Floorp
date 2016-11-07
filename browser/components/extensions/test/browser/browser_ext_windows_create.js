/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      let _checkWindowPromise;
      browser.test.onMessage.addListener(msg => {
        if (msg == "checked-window") {
          _checkWindowPromise.resolve();
          _checkWindowPromise = null;
        }
      });

      let os;

      function checkWindow(expected) {
        return new Promise(resolve => {
          _checkWindowPromise = {resolve};
          browser.test.sendMessage("check-window", expected);
        });
      }

      function createWindow(params, expected, keep = false) {
        return browser.windows.create(params).then(window => {
          for (let key of Object.keys(params)) {
            if (key == "state" && os == "mac" && params.state == "normal") {
              // OS-X doesn't have a hard distinction between "normal" and
              // "maximized" states.
              browser.test.assertTrue(window.state == "normal" || window.state == "maximized",
                                      `Expected window.state (currently ${window.state}) to be "normal" but will accept "maximized"`);
            } else {
              browser.test.assertEq(params[key], window[key], `Got expected value for window.${key}`);
            }
          }

          browser.test.assertEq(1, window.tabs.length, "tabs property got populated");
          return checkWindow(expected).then(() => {
            if (keep) {
              return window;
            }
            if (params.state == "fullscreen" && os == "win") {
              // FIXME: Closing a fullscreen window causes a window leak in
              // Windows tests.
              return browser.windows.update(window.id, {state: "normal"}).then(() => {
                return browser.windows.remove(window.id);
              });
            }
            return browser.windows.remove(window.id);
          });
        });
      }

      browser.runtime.getPlatformInfo().then(info => { os = info.os; })
      .then(() => createWindow({state: "maximized"}, {state: "STATE_MAXIMIZED"}))
      .then(() => createWindow({state: "minimized"}, {state: "STATE_MINIMIZED"}))
      .then(() => createWindow({state: "normal"}, {state: "STATE_NORMAL", hiddenChrome: []}))
      .then(() => createWindow({state: "fullscreen"}, {state: "STATE_FULLSCREEN"}))
      .then(() => {
        return createWindow({type: "popup"},
                            {hiddenChrome: ["menubar", "toolbar", "location", "directories", "status", "extrachrome"],
                             chromeFlags: ["CHROME_OPENAS_DIALOG"]},
                            true);
      }).then(window => {
        return browser.tabs.query({windowType: "popup", active: true}).then(tabs => {
          browser.test.assertEq(1, tabs.length, "Expected only one popup");
          browser.test.assertEq(window.id, tabs[0].windowId, "Expected new window to be returned in query");

          return browser.windows.remove(window.id);
        });
      }).then(() => {
        browser.test.notifyPass("window-create");
      }).catch(e => {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("window-create");
      });
    },
  });

  let latestWindow;
  let windowListener = (window, topic) => {
    if (topic == "domwindowopened") {
      latestWindow = window;
    }
  };
  Services.ww.registerNotification(windowListener);

  extension.onMessage("check-window", expected => {
    if (expected.state != null) {
      let {windowState} = latestWindow;
      if (latestWindow.fullScreen) {
        windowState = latestWindow.STATE_FULLSCREEN;
      }

      if (expected.state == "STATE_NORMAL" && AppConstants.platform == "macosx") {
        ok(windowState == window.STATE_NORMAL || windowState == window.STATE_MAXIMIZED,
           `Expected windowState (currently ${windowState}) to be STATE_NORMAL but will accept STATE_MAXIMIZED`);
      } else {
        is(windowState, window[expected.state],
           `Expected window state to be ${expected.state}`);
      }
    }
    if (expected.hiddenChrome) {
      let chromeHidden = latestWindow.document.documentElement.getAttribute("chromehidden");
      is(chromeHidden.trim().split(/\s+/).sort().join(" "),
         expected.hiddenChrome.sort().join(" "),
         "Got expected hidden chrome");
    }
    if (expected.chromeFlags) {
      let {chromeFlags} = latestWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIDocShell)
                                      .treeOwner.QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIXULWindow);
      for (let flag of expected.chromeFlags) {
        ok(chromeFlags & Ci.nsIWebBrowserChrome[flag],
           `Expected window to have the ${flag} flag`);
      }
    }

    extension.sendMessage("checked-window");
  });

  yield extension.startup();
  yield extension.awaitFinish("window-create");
  yield extension.unload();

  Services.ww.unregisterNotification(windowListener);
  latestWindow = null;
});
