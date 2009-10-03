/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

    let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        if (aTopic == "domwindowopened") {
          ww.unregisterNotification(this);

          let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
          win.addEventListener("load", function() {
            win.removeEventListener("load", arguments.callee, false);

            let browser = win.gBrowser;
            browser.addEventListener("load", function() {
              browser.removeEventListener("load", arguments.callee, true);
              
              // view source window is loaded, proceed with the rest of the test
              step1();
            }, true);
          }, false);
        }
      }
    };
    ww.registerNotification(observer);

    openViewSource();

    function openViewSource() {
      // invoke the View Source command
      document.getElementById("View:PageSource").doCommand();
    }

    function step1() {
      observer = {
        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "domwindowclosed") {
            ok(true, "Entering the private browsing mode should close the view source window");
            ww.unregisterNotification(observer);

            step2();
          }
          else if (aTopic == "domwindowopened")
            ok(false, "Entering the private browsing mode should not open any view source window");
        }
      };
      ww.registerNotification(observer);

      gBrowser.addTabsProgressListener({
        onLocationChange: function() {},
        onProgressChange: function() {},
        onSecurityChange: function() {},
        onStatusChange: function() {},
        onRefreshAttempted: function() {},
        onLinkIconAvailable: function() {},
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
      observer = {
        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "domwindowopened") {
            ww.unregisterNotification(this);

            let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
            win.addEventListener("load", function() {
              win.removeEventListener("load", arguments.callee, false);

              let browser = win.gBrowser;
              browser.addEventListener("load", function() {
                browser.removeEventListener("load", arguments.callee, true);
                
                // view source window inside private browsing mode opened
                step5();
              }, true);
            }, false);
          }
        }
      };
      ww.registerNotification(observer);

      openViewSource();
    }

    function step5() {
      let events = 0;

      observer = {
        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "domwindowclosed") {
            ok(true, "Leaving the private browsing mode should close the existing view source window");
            if (++events == 2)
              ww.unregisterNotification(observer);
          }
          else if (aTopic == "domwindowopened") {
            ok(true, "Leaving the private browsing mode should restore the previous view source window");
            if (++events == 2)
              ww.unregisterNotification(observer);

            let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
            win.addEventListener("load", function() {
              win.removeEventListener("load", arguments.callee, false);

              let browser = win.gBrowser;
              browser.addEventListener("load", function() {
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
      };
      ww.registerNotification(observer);

      // exit private browsing mode
      pb.privateBrowsingEnabled = false;
    }
  }, true);
  aboutBrowser.loadURI("about:");
}
