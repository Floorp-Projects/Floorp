/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  testRunner.runTests();
}

var testRunner = {

  tests:
    [
      {
        test: function(params) {
          params.url.value = "test.com";
          params.btnAllow.doCommand();
          is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission text should be set correctly");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", host: "test.com", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test: function(params) {
          params.url.value = "test.com";
          params.btnBlock.doCommand();
          is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
                                          "permission should change to deny in UI");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", host: "test.com", data: "changed",
                        capability: Ci.nsIPermissionManager.DENY_ACTION  }],
      },
      {
        test: function(params) {
          params.url.value = "test.com";
          params.btnAllow.doCommand();
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission should revert back to allow");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", host: "test.com", data: "changed",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test: function(params) {
          params.url.value = "test.com";
          params.btnRemove.doCommand();
          is(params.tree.view.rowCount, 0, "exception should be removed");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", host: "test.com", data: "deleted" }],
      },
      {
        test: function(params) {
          let uri = params.ioService.newURI("http://test.com", null, null);
          params.pm.add(uri, "popup", Ci.nsIPermissionManager.DENY_ACTION);
          is(params.tree.view.rowCount, 0, "adding unrelated permission should not change display");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "popup", host: "test.com", data: "added",
                        capability: Ci.nsIPermissionManager.DENY_ACTION }],
        cleanUp: function(params) {
          params.pm.remove("test.com", "popup");
        },
      },
    ],

  _currentTest: -1,

  runTests: function() {
    this._currentTest++;

    info("Running test #" + (this._currentTest + 1) + "\n");
    let that = this;
    let p = this.runCurrentTest();
    p.then(function() {
      if (that._currentTest == that.tests.length - 1) {
        finish();
      }
      else {
        that.runTests();
      }
    });
  },

  runCurrentTest: function() {
    return new Promise(function(resolve, reject) {

      let helperFunctions = {

        prefWindowObserver: function(subject, topic, data) {
          if (topic != "domwindowopened")
            return;

          Services.ww.unregisterNotification(helperFunctions.prefWindowObserver);

          let win = subject.QueryInterface(Ci.nsIDOMEventTarget);

          win.addEventListener("load", function(event) {
            let historyMode = event.target.getElementById("historyMode");
            historyMode.value = "custom";
            historyMode.doCommand();
            Services.ww.registerNotification(helperFunctions.cookiesWindowObserver);
            event.target.getElementById("cookieExceptions").doCommand();
          }, false);
        },

        cookiesWindowObserver: function(subject, topic, data) {
          if (topic != "domwindowopened")
            return;

          Services.ww.unregisterNotification(helperFunctions.cookiesWindowObserver);

          let win = subject.QueryInterface(Ci.nsIDOMEventTarget);

          win.addEventListener("load", function(event) {
            SimpleTest.executeSoon(function() helperFunctions.windowLoad(event, win));
          }, false);
        },

        windowLoad: function(event, win) {
          let params = {
            doc: event.target,
            tree: event.target.getElementById("permissionsTree"),
            statusCol: event.target.getElementById("permissionsTree").treeBoxObject.columns.getColumnAt(1),
            url: event.target.getElementById("url"),
            btnAllow: event.target.getElementById("btnAllow"),
            btnBlock: event.target.getElementById("btnBlock"),
            btnApplyChanges: event.target.getElementById("btnApplyChanges"),
            btnRemove: event.target.getElementById("removePermission"),
            pm: Cc["@mozilla.org/permissionmanager;1"]
                       .getService(Ci.nsIPermissionManager),
            ioService: Cc["@mozilla.org/network/io-service;1"]
                              .getService(Ci.nsIIOService),
            allowText: win.gPermissionManager._getCapabilityString(
                                Ci.nsIPermissionManager.ALLOW_ACTION),
            denyText: win.gPermissionManager._getCapabilityString(
                               Ci.nsIPermissionManager.DENY_ACTION),
            allow: Ci.nsIPermissionManager.ALLOW_ACTION,
            deny: Ci.nsIPermissionManager.DENY_ACTION,
          };

          let permObserver = {
            observe: function(aSubject, aTopic, aData) {
              if (aTopic != "perm-changed")
                return;

              if (testRunner.tests[testRunner._currentTest].observances.length == 0) {
                // Should fail here as we are not expecting a notification, but we don't.
                // See bug 1063410.
                return;
              }

              let permission = aSubject.QueryInterface(Ci.nsIPermission);
              let expected = testRunner.tests[testRunner._currentTest].observances.shift();

              is(aData, expected.data, "type of message should be the same");
              for each (let prop in ["type", "host", "capability"]) {
                if (expected[prop])
                  is(permission[prop], expected[prop],
                    "property: \"" + prop  + "\" should be equal");
              }

              os.removeObserver(permObserver, "perm-changed");

              if (testRunner.tests[testRunner._currentTest].cleanup) {
                testRunner.tests[testRunner._currentTest].cleanup();
              }

              testRunner.dialog.close(params);
              win.close();
              resolve();
            },
          };

          let os = Cc["@mozilla.org/observer-service;1"]
                     .getService(Ci.nsIObserverService);

          os.addObserver(permObserver, "perm-changed", false);

          if (testRunner._currentTest == 0) {
            is(params.tree.view.rowCount, 0, "no cookie exceptions");
          }

          testRunner.tests[testRunner._currentTest].test(params);
        },
      };

      Services.ww.registerNotification(helperFunctions.prefWindowObserver);

      testRunner.dialog = openDialog("chrome://browser/content/preferences/preferences.xul",
                                     "Preferences", "chrome,titlebar,toolbar,centerscreen,dialog=no",
                                     "panePrivacy");
    });
  },
};
