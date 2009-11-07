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
 * Nochum Sossonko.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nochum Sossonko <highmind63@gmail.com> (Original Author)
 *   Ehsan Akhgari <ehsan@mozilla.com>
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

// This test makes sure that cancelling the unloading of a page with a beforeunload
// handler prevents the private browsing mode transition.

function test() {
  const kTestPage1 = "data:text/html,<body%20onbeforeunload='return%20false;'>first</body>";
  const kTestPage2 = "data:text/html,<body%20onbeforeunload='return%20false;'>second</body>";
  let pb = Cc["@mozilla.org/privatebrowsing;1"]
             .getService(Ci.nsIPrivateBrowsingService);

  let promptHelper = {
    rejectDialog: 0,
    acceptDialog: 0,
    confirmCalls: 0,

    observe: function(aSubject, aTopic, aData) {
      let dialogWin = aSubject.QueryInterface(Ci.nsIDOMWindow);
      this.confirmCalls++;
      let button;
      if (this.acceptDialog-- > 0)
        button = dialogWin.document.documentElement.getButton("accept").click();
      else if (this.rejectDialog-- > 0)
        button = dialogWin.document.documentElement.getButton("cancel").click();
    }
  };

  Cc["@mozilla.org/observer-service;1"]
    .getService(Ci.nsIObserverService)
    .addObserver(promptHelper, "common-dialog-loaded", false);

  waitForExplicitFinish();
  let browser1 = gBrowser.getBrowserForTab(gBrowser.addTab());
  browser1.addEventListener("load", function() {
    browser1.removeEventListener("load", arguments.callee, true);

    let browser2 = gBrowser.getBrowserForTab(gBrowser.addTab());
    browser2.addEventListener("load", function() {
      browser2.removeEventListener("load", arguments.callee, true);

      promptHelper.rejectDialog = 1;
      pb.privateBrowsingEnabled = true;

      ok(!pb.privateBrowsingEnabled, "Private browsing mode should not have been activated");
      is(promptHelper.confirmCalls, 1, "Only one confirm box should be shown");
      is(gBrowser.tabContainer.childNodes.length, 3,
         "No tabs should be closed because private browsing mode transition was canceled");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild).currentURI.spec, "about:blank",
         "The first tab should be a blank tab");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild.nextSibling).currentURI.spec, kTestPage1,
         "The middle tab should be the same one we opened");
      is(gBrowser.getBrowserForTab(gBrowser.tabContainer.lastChild).currentURI.spec, kTestPage2,
         "The last tab should be the same one we opened");
      is(promptHelper.rejectDialog, 0, "Only one confirm dialog should have been rejected");

      promptHelper.confirmCalls = 0;
      promptHelper.acceptDialog = 2;
      pb.privateBrowsingEnabled = true;

      ok(pb.privateBrowsingEnabled, "Private browsing mode should have been activated");
      is(promptHelper.confirmCalls, 2, "Only two confirm boxes should be shown");
      is(gBrowser.tabContainer.childNodes.length, 1,
         "Incorrect number of tabs after transition into private browsing");
      gBrowser.selectedBrowser.addEventListener("load", function() {
        gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

        is(gBrowser.selectedBrowser.currentURI.spec, "about:privatebrowsing",
           "Incorrect page displayed after private browsing transition");
        is(promptHelper.acceptDialog, 0, "Two confirm dialogs should have been accepted");

        gBrowser.selectedBrowser.addEventListener("load", function() {
          gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

          gBrowser.selectedTab = gBrowser.addTab();
          gBrowser.selectedBrowser.addEventListener("load", function() {
            gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

            promptHelper.confirmCalls = 0;
            promptHelper.rejectDialog = 1;
            pb.privateBrowsingEnabled = false;

            ok(pb.privateBrowsingEnabled, "Private browsing mode should not have been deactivated");
            is(promptHelper.confirmCalls, 1, "Only one confirm box should be shown");
            is(gBrowser.tabContainer.childNodes.length, 2,
               "No tabs should be closed because private browsing mode transition was canceled");
            is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild).currentURI.spec, kTestPage1,
               "The first tab should be the same one we opened");
            is(gBrowser.getBrowserForTab(gBrowser.tabContainer.lastChild).currentURI.spec, kTestPage2,
               "The last tab should be the same one we opened");
            is(promptHelper.rejectDialog, 0, "Only one confirm dialog should have been rejected");

            promptHelper.confirmCalls = 0;
            promptHelper.acceptDialog = 2;
            pb.privateBrowsingEnabled = false;

            ok(!pb.privateBrowsingEnabled, "Private browsing mode should have been deactivated");
            is(promptHelper.confirmCalls, 2, "Only two confirm boxes should be shown");
            is(gBrowser.tabContainer.childNodes.length, 3,
               "Incorrect number of tabs after transition into private browsing");

            let loads = 0;
            function waitForLoad(event) {
              gBrowser.removeEventListener("load", arguments.callee, true);

              if (++loads != 3)
                return;

              is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild).currentURI.spec, "about:blank",
                 "The first tab should be a blank tab");
              is(gBrowser.getBrowserForTab(gBrowser.tabContainer.firstChild.nextSibling).currentURI.spec, kTestPage1,
                 "The middle tab should be the same one we opened");
              is(gBrowser.getBrowserForTab(gBrowser.tabContainer.lastChild).currentURI.spec, kTestPage2,
                 "The last tab should be the same one we opened");
                      is(promptHelper.acceptDialog, 0, "Two confirm dialogs should have been accepted");
              is(promptHelper.acceptDialog, 0, "Two prompts should have been raised");

              promptHelper.acceptDialog = 2;
              gBrowser.removeTab(gBrowser.tabContainer.lastChild);
              gBrowser.removeTab(gBrowser.tabContainer.lastChild);

              finish();
            }
            for (let i = 0; i < gBrowser.browsers.length; ++i)
              gBrowser.browsers[i].addEventListener("load", waitForLoad, true);
          }, true);
          gBrowser.selectedBrowser.loadURI(kTestPage2);
        }, true);
        gBrowser.selectedBrowser.loadURI(kTestPage1);
      }, true);
    }, true);
    browser2.loadURI(kTestPage2);
  }, true);
  browser1.loadURI(kTestPage1);
}
