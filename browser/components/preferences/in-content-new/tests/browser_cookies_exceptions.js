/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

function test() {
  waitForExplicitFinish();
  requestLongerTimeout(3);
  testRunner.runTests();
}

var testRunner = {

  tests:
    [
      {
        test(params) {
          params.url.value = "test.com";
          params.btnAllow.doCommand();
          is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
          is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission text should be set correctly");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://test.com", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "test.com";
          params.btnBlock.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
                                          "permission should change to deny in UI");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://test.com", data: "changed",
                        capability: Ci.nsIPermissionManager.DENY_ACTION  }],
      },
      {
        test(params) {
          params.url.value = "test.com";
          params.btnAllow.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission should revert back to allow");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://test.com", data: "changed",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "test.com";
          params.btnRemove.doCommand();
          is(params.tree.view.rowCount, 0, "exception should be removed");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://test.com", data: "deleted" }],
      },
      {
        expectPermObservancesDuringTestFunction: true,
        test(params) {
          let uri = params.ioService.newURI("http://test.com");
          params.pm.add(uri, "popup", Ci.nsIPermissionManager.DENY_ACTION);
          is(params.tree.view.rowCount, 0, "adding unrelated permission should not change display");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "popup", origin: "http://test.com", data: "added",
                        capability: Ci.nsIPermissionManager.DENY_ACTION }],
        cleanUp(params) {
          let uri = params.ioService.newURI("http://test.com");
          params.pm.remove(uri, "popup");
        },
      },
      {
        test(params) {
          params.url.value = "https://test.com:12345";
          params.btnAllow.doCommand();
          is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
          is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission text should be set correctly");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "https://test.com:12345", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "https://test.com:12345";
          params.btnBlock.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
                                          "permission should change to deny in UI");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
                        capability: Ci.nsIPermissionManager.DENY_ACTION  }],
      },
      {
        test(params) {
          params.url.value = "https://test.com:12345";
          params.btnAllow.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission should revert back to allow");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "https://test.com:12345";
          params.btnRemove.doCommand();
          is(params.tree.view.rowCount, 0, "exception should be removed");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "https://test.com:12345", data: "deleted" }],
      },
      {
        test(params) {
          params.url.value = "localhost:12345";
          params.btnAllow.doCommand();
          is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
          is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission text should be set correctly");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://localhost:12345", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "localhost:12345";
          params.btnBlock.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
                                          "permission should change to deny in UI");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
                        capability: Ci.nsIPermissionManager.DENY_ACTION  }],
      },
      {
        test(params) {
          params.url.value = "localhost:12345";
          params.btnAllow.doCommand();
          is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
                                          "origin name should be set correctly");
          is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
                                          "permission should revert back to allow");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
      },
      {
        test(params) {
          params.url.value = "localhost:12345";
          params.btnRemove.doCommand();
          is(params.tree.view.rowCount, 0, "exception should be removed");
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://localhost:12345", data: "deleted" }],
      },
      {
        expectPermObservancesDuringTestFunction: true,
        test(params) {
          for (let URL of ["http://a", "http://z", "http://b"]) {
            let URI = params.ioService.newURI(URL);
            params.pm.add(URI, "cookie", Ci.nsIPermissionManager.ALLOW_ACTION);
          }

          is(params.tree.view.rowCount, 3, "Three permissions should be present");
          is(params.tree.view.getCellText(0, params.nameCol), "http://a",
             "site should be sorted. 'a' should be first");
          is(params.tree.view.getCellText(1, params.nameCol), "http://b",
             "site should be sorted. 'b' should be second");
          is(params.tree.view.getCellText(2, params.nameCol), "http://z",
             "site should be sorted. 'z' should be third");

          // Sort descending then check results in cleanup since sorting isn't synchronous.
          EventUtils.synthesizeMouseAtCenter(params.doc.getElementById("siteCol"), {},
                                             params.doc.defaultView);
          params.btnApplyChanges.doCommand();
        },
        observances: [{ type: "cookie", origin: "http://a", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
                      { type: "cookie", origin: "http://z", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
                      { type: "cookie", origin: "http://b", data: "added",
                        capability: Ci.nsIPermissionManager.ALLOW_ACTION }],
        cleanUp(params) {
          is(params.tree.view.getCellText(0, params.nameCol), "http://z",
             "site should be sorted. 'z' should be first");
          is(params.tree.view.getCellText(1, params.nameCol), "http://b",
             "site should be sorted. 'b' should be second");
          is(params.tree.view.getCellText(2, params.nameCol), "http://a",
             "site should be sorted. 'a' should be third");

          for (let URL of ["http://a", "http://z", "http://b"]) {
            let uri = params.ioService.newURI(URL);
            params.pm.remove(uri, "cookie");
          }
        },
      },
    ],

  _currentTest: -1,

  runTests() {
    this._currentTest++;

    info("Running test #" + (this._currentTest + 1) + "\n");
    let that = this;
    let p = this.runCurrentTest(this._currentTest + 1);
    p.then(function() {
      if (that._currentTest == that.tests.length - 1) {
        finish();
      } else {
        that.runTests();
      }
    });
  },

  runCurrentTest(testNumber) {
    return new Promise(function(resolve, reject) {

      let helperFunctions = {
        windowLoad(win) {
          let doc = win.document;
          let params = {
            doc,
            tree: doc.getElementById("permissionsTree"),
            nameCol: doc.getElementById("permissionsTree").treeBoxObject.columns.getColumnAt(0),
            statusCol: doc.getElementById("permissionsTree").treeBoxObject.columns.getColumnAt(1),
            url: doc.getElementById("url"),
            btnAllow: doc.getElementById("btnAllow"),
            btnBlock: doc.getElementById("btnBlock"),
            btnApplyChanges: doc.getElementById("btnApplyChanges"),
            btnRemove: doc.getElementById("removePermission"),
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
            observe(aSubject, aTopic, aData) {
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
              for (let prop of ["type", "capability"]) {
                if (expected[prop])
                  is(permission[prop], expected[prop],
                    "property: \"" + prop + "\" should be equal");
              }

              if (expected.origin) {
                is(permission.principal.origin, expected.origin,
                   "property: \"origin\" should be equal");
              }

              os.removeObserver(permObserver, "perm-changed");

              let testCase = testRunner.tests[testRunner._currentTest];
              if (!testCase.expectPermObservancesDuringTestFunction) {
                if (testCase.cleanUp) {
                  testCase.cleanUp(params);
                }

                gBrowser.removeCurrentTab();
                resolve();
              }
            },
          };

          let os = Cc["@mozilla.org/observer-service;1"]
                     .getService(Ci.nsIObserverService);

          os.addObserver(permObserver, "perm-changed");

          if (testRunner._currentTest == 0) {
            is(params.tree.view.rowCount, 0, "no cookie exceptions");
          }

          try {
            let testCase = testRunner.tests[testRunner._currentTest];
            testCase.test(params);
            if (testCase.expectPermObservancesDuringTestFunction) {
              if (testCase.cleanUp) {
                testCase.cleanUp(params);
              }

              gBrowser.removeCurrentTab();
              resolve();
            }
          } catch (ex) {
            ok(false, "exception while running test #" +
                      testNumber + ": " + ex);
          }
        },
      };

      registerCleanupFunction(function() {
        Services.prefs.clearUserPref("privacy.history.custom");
      });

      openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true}).then(function() {
        let doc = gBrowser.contentDocument;
        let historyMode = doc.getElementById("historyMode");
        historyMode.value = "custom";
        historyMode.doCommand();
        doc.getElementById("cookieExceptions").doCommand();

        let subDialogURL = "chrome://browser/content/preferences/permissions.xul";
        promiseLoadSubDialog(subDialogURL).then(function(win) {
          helperFunctions.windowLoad(win);
        });
      });
    });
  },
};
