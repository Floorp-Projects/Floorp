/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that Open Location dialog is usable inside the private browsing
// mode without leaving any trace of the URLs visited.

function test() {
  // initialization
  gPrefService.setBoolPref("browser.privatebrowsing.keep_current_session", true);
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  waitForExplicitFinish();

  function openLocation(url, autofilled, callback) {
    function observer(aSubject, aTopic, aData) {
      switch (aTopic) {
        case "domwindowopened":
          let dialog = aSubject.QueryInterface(Ci.nsIDOMWindow);
          dialog.addEventListener("load", function () {
            dialog.removeEventListener("load", arguments.callee, false);

            let browser = gBrowser.selectedBrowser;
            browser.addEventListener("load", function() {
              browser.removeEventListener("load", arguments.callee, true);

              is(browser.currentURI.spec, url,
                 "The correct URL should be loaded via the open location dialog");
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

    Services.ww.registerNotification(observer);
    gPrefService.setIntPref("general.open_location.last_window_choice", 0);
    openDialog("chrome://browser/content/openLocation.xul", "_blank",
               "chrome,titlebar", window);
  }


  if (gPrefService.prefHasUserValue("general.open_location.last_url"))
    gPrefService.clearUserPref("general.open_location.last_url");

  openLocation("http://example.com/", "", function() {
    openLocation("http://example.org/", "http://example.com/", function() {
      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
      openLocation("about:logo", "", function() {
        openLocation("about:buildconfig", "about:logo", function() {
          // exit private browsing mode
          pb.privateBrowsingEnabled = false;
          openLocation("about:blank", "http://example.org/", function() {
            gPrefService.clearUserPref("general.open_location.last_url");
            if (gPrefService.prefHasUserValue("general.open_location.last_window_choice"))
              gPrefService.clearUserPref("general.open_location.last_window_choice");
            gPrefService.clearUserPref("browser.privatebrowsing.keep_current_session");
            finish();
          });
        });
      });
    });
  });
}
