/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test makes sure that private browsing toggles correctly via the -private
// command line argument.

function test() {
  // initialization
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  waitForExplicitFinish();

  function simulatePrivateCommandLineArgument() {
    function testprivatecl() {
    }

    testprivatecl.prototype = {
      _arguments: ["private-toggle"],
      get length() {
        return this._arguments.length;
      },
      getArgument: function getArgument(aIndex) {
        return this._arguments[aIndex];
      },
      findFlag: function findFlag(aFlag, aCaseSensitive) {
        for (let i = 0; i < this._arguments.length; ++i)
          if (aCaseSensitive ?
              (this._arguments[i] == aFlag) :
              (this._arguments[i].toLowerCase() == aFlag.toLowerCase()))
            return i;
        return -1;
      },
      removeArguments: function removeArguments(aStart, aEnd) {
        this._arguments.splice(aStart, aEnd - aStart + 1);
      },
      handleFlag: function handleFlag (aFlag, aCaseSensitive) {
        let res = this.findFlag(aFlag, aCaseSensitive);
        if (res > -1) {
          this.removeArguments(res, res);
          return true;
        }
        return false;
      },
      handleFlagWithParam: function handleFlagWithParam(aFlag, aCaseSensitive) {
        return null;
      },
      STATE_INITIAL_LAUNCH: 0,
      STATE_REMOTE_AUTO: 1,
      STATE_REMOTE_EXPLICIT: 2,
      get state() {
        return this.STATE_REMOTE_AUTO;
      },
      preventDefault: false,
      workingDirectory: null,
      windowContext: null,
      resolveFile: function resolveFile (aArgument) {
        return null;
      },
      resolveURI: function resolveURI (aArgument) {
        return null;
      },
      QueryInterface: function(aIID) {
        if (!aIID.equals(Ci.nsICommandLine)
            && !aIID.equals(Ci.nsISupports))
          throw Cr.NS_ERROR_NO_INTERFACE;
        return this;
      }
    };

    let testcl = new testprivatecl();

    let catMan = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
    let categories = catMan.enumerateCategory("command-line-handler");
    while (categories.hasMoreElements()) {
      let category = categories.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let contractID = catMan.getCategoryEntry("command-line-handler", category);
      let handler = Cc[contractID].getService(Ci.nsICommandLineHandler);
      handler.handle(testcl);
    }
  }

  function observer(aSubject, aTopic, aData) {
    isnot(aTopic, "domwindowopened", "The -private-toggle argument should be silent");
  }
  Services.ww.registerNotification(observer);

  let tab = gBrowser.selectedTab;
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function () {
    browser.removeEventListener("load", arguments.callee, true);
    ok(!pb.privateBrowsingEnabled, "The private browsing mode should not be started");
    is(browser.contentWindow.location, "about:", "The correct page has been loaded");

    simulatePrivateCommandLineArgument();
    is(pb.lastChangedByCommandLine, true,
       "The status change reason should reflect the PB mode being set from the command line");
    tab = gBrowser.selectedTab;
    browser = gBrowser.getBrowserForTab(tab);
    browser.addEventListener("load", function() {
      browser.removeEventListener("load", arguments.callee, true);
      ok(pb.privateBrowsingEnabled, "The private browsing mode should be started");
      is(browser.contentWindow.location, "about:privatebrowsing",
         "about:privatebrowsing should now be loaded");

      simulatePrivateCommandLineArgument();
      is(pb.lastChangedByCommandLine, true,
         "The status change reason should reflect the PB mode being set from the command line");
      tab = gBrowser.selectedTab;
      browser = gBrowser.getBrowserForTab(tab);
      browser.addEventListener("load", function() {
        browser.removeEventListener("load", arguments.callee, true);
        ok(!pb.privateBrowsingEnabled, "The private browsing mode should be stopped");
        is(browser.contentWindow.location, "about:",
           "about: should now be loaded");

        let newTab = gBrowser.addTab();
        gBrowser.removeTab(tab);
        Services.ww.unregisterNotification(observer);
        finish();
      }, true);
    }, true);
  }, true);
  browser.loadURI("about:");
}
