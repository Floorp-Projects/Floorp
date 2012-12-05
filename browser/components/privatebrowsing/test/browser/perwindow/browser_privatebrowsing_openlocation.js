/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that Open Location dialog is usable inside the private browsing
// mode without leaving any trace of the URLs visited.

function test() {
  // initialization
  waitForExplicitFinish();

  function openLocation(aWindow, url, autofilled, callback) {
    function observer(aSubject, aTopic, aData) {
      switch (aTopic) {
        case "domwindowopened":
          let dialog = aSubject.QueryInterface(Ci.nsIDOMWindow);
          dialog.addEventListener("load", function () {
            dialog.removeEventListener("load", arguments.callee, false);

            let browser = aWindow.gBrowser.selectedBrowser;
            browser.addEventListener("load", function() {
              // Ignore non-related loads (could be about:privatebrowsing for example, see bug 817932)
              if (browser.currentURI.spec != url) {
                return;
              }

              browser.removeEventListener("load", arguments.callee, true);

              executeSoon(callback);
            }, true);

            SimpleTest.waitForFocus(function() {
              let input = dialog.document.getElementById("dialog.input");
              is(input.value, autofilled, "The input field should be correctly auto-filled");
              input.focus();
              for (let i = 0; i < url.length; ++i)
                EventUtils.synthesizeKey(url[i], {}, dialog);
              EventUtils.synthesizeKey("VK_RETURN", {}, dialog);
            }, dialog);
          }, false);
          break;

        case "domwindowclosed":
          Services.ww.unregisterNotification(arguments.callee);
          break;
      }
    }

    executeSoon(function() {
      Services.ww.registerNotification(observer);
      gPrefService.setIntPref("general.open_location.last_window_choice", 0);
      aWindow.openDialog("chrome://browser/content/openLocation.xul", "_blank",
                         "chrome,titlebar", aWindow);
    });
  }

  let windowsToClose = [];
  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      windowsToClose.push(win);
      callback(win);
    }, false);
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  if (gPrefService.prefHasUserValue("general.open_location.last_url"))
    gPrefService.clearUserPref("general.open_location.last_url");

  testOnWindow({private: false}, function(win) {
    openLocation(win, "http://example.com/", "", function() {
      testOnWindow({private: false}, function(win) {
        openLocation(win, "http://example.org/", "http://example.com/", function() {
          testOnWindow({private: true}, function(win) {
            openLocation(win, "about:logo", "", function() {
                testOnWindow({private: true}, function(win) {
                  openLocation(win, "about:buildconfig", "about:logo", function() {
                    testOnWindow({private: false}, function(win) {
                      openLocation(win, "about:blank", "http://example.org/", function() {
                        gPrefService.clearUserPref("general.open_location.last_url");
                        if (gPrefService.prefHasUserValue("general.open_location.last_window_choice"))
                          gPrefService.clearUserPref("general.open_location.last_window_choice");
                        finish();
                       });
                     });
                  });
               });
            });
          });
        });
      });
    });
  });
}
