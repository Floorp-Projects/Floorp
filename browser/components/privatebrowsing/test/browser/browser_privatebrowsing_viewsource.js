/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that entering the private browsing mode closes
// all view source windows, and leaving it restores them

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  let aboutBrowser = gBrowser.selectedBrowser;
  aboutBrowser.addEventListener("load", function () {
    aboutBrowser.removeEventListener("load", arguments.callee, true);

    function observer(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(observer);

      let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
      win.addEventListener("load", function () {
        win.removeEventListener("load", arguments.callee, false);

        let browser = win.gBrowser;
        browser.addEventListener("load", function () {
          browser.removeEventListener("load", arguments.callee, true);
          
          // view source window is loaded, proceed with the rest of the test
          step1();
        }, true);
      }, false);
    }
    Services.ww.registerNotification(observer);

    openViewSource();

    function openViewSource() {
      // invoke the View Source command
      document.getElementById("View:PageSource").doCommand();
    }

    function step1() {
      function observer(aSubject, aTopic, aData) {
        if (aTopic == "domwindowclosed") {
          ok(true, "Entering the private browsing mode should close the view source window");
          Services.ww.unregisterNotification(observer);

          step2();
        }
        else if (aTopic == "domwindowopened")
          ok(false, "Entering the private browsing mode should not open any view source window");
      }
      Services.ww.registerNotification(observer);

      gBrowser.addTabsProgressListener({
        onStateChange: function(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
          if (aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP |
                             Ci.nsIWebProgressListener.STATE_IS_WINDOW)) {
            gBrowser.removeTabsProgressListener(this);

            step3();
          }
        }
      });

      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
    }

    let events = 0, step2, step3;
    step2 = step3 = function() {
      if (++events == 2)
        step4();
    }

    function step4() {
      function observer(aSubject, aTopic, aData) {
        if (aTopic != "domwindowopened")
          return;

        Services.ww.unregisterNotification(observer);

        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win.addEventListener("load", function () {
          win.removeEventListener("load", arguments.callee, false);

          let browser = win.gBrowser;
          browser.addEventListener("load", function () {
            browser.removeEventListener("load", arguments.callee, true);
            
            // view source window inside private browsing mode opened
            step5();
          }, true);
        }, false);
      }
      Services.ww.registerNotification(observer);

      openViewSource();
    }

    function step5() {
      let events = 0;

      function observer(aSubject, aTopic, aData) {
        if (aTopic == "domwindowclosed") {
          ok(true, "Leaving the private browsing mode should close the existing view source window");
          if (++events == 2)
            Services.ww.unregisterNotification(observer);
        }
        else if (aTopic == "domwindowopened") {
          ok(true, "Leaving the private browsing mode should restore the previous view source window");
          if (++events == 2)
            Services.ww.unregisterNotification(observer);

          let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
          win.addEventListener("load", function () {
            win.removeEventListener("load", arguments.callee, false);

            let browser = win.gBrowser;
            browser.addEventListener("load", function () {
              browser.removeEventListener("load", arguments.callee, true);
              
              is(win.content.location.href, "view-source:about:",
                "The correct view source window should be restored");

              // cleanup
              win.close();
              gBrowser.removeCurrentTab();
              finish();
            }, true);
          }, false);
        }
      }
      Services.ww.registerNotification(observer);

      // exit private browsing mode
      pb.privateBrowsingEnabled = false;
    }
  }, true);
  aboutBrowser.loadURI("about:");
}
