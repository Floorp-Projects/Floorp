/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(3);

add_task(async function testAllow() {
  await runTest(async (params, observeAllPromise, apply) => {
    is(params.tree.view.rowCount, 0, "no cookie exceptions");

    params.url.value = "test.com";
    params.btnAllow.doCommand();
    is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
    is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission text should be set correctly");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlock() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "test.com";
    params.btnBlock.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
       "permission should change to deny in UI");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgain() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "test.com";
    params.btnAllow.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "http://test.com",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission should revert back to allow");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemove() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "test.com";
    params.btnRemove.doCommand();
    is(params.tree.view.rowCount, 0, "exception should be removed");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://test.com", data: "deleted" }]);
});

add_task(async function testAdd() {
  await runTest(async (params, observeAllPromise, apply) => {
    let uri = Services.io.newURI("http://test.com");
    Services.perms.add(uri, "popup", Ci.nsIPermissionManager.DENY_ACTION);
    is(params.tree.view.rowCount, 0, "adding unrelated permission should not change display");

    apply();
    await observeAllPromise;

    Services.perms.remove(uri, "popup");
  }, [{ type: "popup", origin: "http://test.com", data: "added",
        capability: Ci.nsIPermissionManager.DENY_ACTION }]);
});

add_task(async function testAllowHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnAllow.doCommand();
    is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
    is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission text should be set correctly");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlockHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnBlock.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
       "permission should change to deny in UI");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgainHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnAllow.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "https://test.com:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission should revert back to allow");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemoveHTTPSWithPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "https://test.com:12345";
    params.btnRemove.doCommand();
    is(params.tree.view.rowCount, 0, "exception should be removed");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "https://test.com:12345", data: "deleted" }]);
});

add_task(async function testAllowPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnAllow.doCommand();
    is(params.tree.view.rowCount, 1, "added exception shows up in treeview");
    is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission text should be set correctly");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testBlockPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnBlock.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.denyText,
       "permission should change to deny in UI");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
        capability: Ci.nsIPermissionManager.DENY_ACTION  }]);
});

add_task(async function testAllowAgainPort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnAllow.doCommand();
    is(params.tree.view.getCellText(0, params.nameCol), "http://localhost:12345",
       "origin name should be set correctly");
    is(params.tree.view.getCellText(0, params.statusCol), params.allowText,
       "permission should revert back to allow");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "changed",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

add_task(async function testRemovePort() {
  await runTest(async (params, observeAllPromise, apply) => {
    params.url.value = "localhost:12345";
    params.btnRemove.doCommand();
    is(params.tree.view.rowCount, 0, "exception should be removed");

    apply();
    await observeAllPromise;
  }, [{ type: "cookie", origin: "http://localhost:12345", data: "deleted" }]);
});

add_task(async function testSort() {
  await runTest(async (params, observeAllPromise, apply) => {
    for (let URL of ["http://a", "http://z", "http://b"]) {
      let URI = Services.io.newURI(URL);
      Services.perms.add(URI, "cookie", Ci.nsIPermissionManager.ALLOW_ACTION);
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

    is(params.tree.view.getCellText(0, params.nameCol), "http://z",
       "site should be sorted. 'z' should be first");
    is(params.tree.view.getCellText(1, params.nameCol), "http://b",
       "site should be sorted. 'b' should be second");
    is(params.tree.view.getCellText(2, params.nameCol), "http://a",
       "site should be sorted. 'a' should be third");

    apply();
    await observeAllPromise;

    for (let URL of ["http://a", "http://z", "http://b"]) {
      let uri = Services.io.newURI(URL);
      Services.perms.remove(uri, "cookie");
    }
  }, [{ type: "cookie", origin: "http://a", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
      { type: "cookie", origin: "http://z", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION },
      { type: "cookie", origin: "http://b", data: "added",
        capability: Ci.nsIPermissionManager.ALLOW_ACTION }]);
});

async function runTest(test, observances) {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.history.custom");
  });

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});

  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let doc = gBrowser.contentDocument;
  let historyMode = doc.getElementById("historyMode");
  historyMode.value = "custom";
  historyMode.doCommand();

  let promiseSubDialogLoaded =
      promiseLoadSubDialog("chrome://browser/content/preferences/permissions.xul");
  doc.getElementById("cookieExceptions").doCommand();

  let win = await promiseSubDialogLoaded;

  doc = win.document;
  let params = {
    doc,
    tree: doc.getElementById("permissionsTree"),
    nameCol: doc.getElementById("permissionsTree").treeBoxObject.columns.getColumnAt(0),
    statusCol: doc.getElementById("permissionsTree").treeBoxObject.columns.getColumnAt(1),
    url: doc.getElementById("url"),
    btnAllow: doc.getElementById("btnAllow"),
    btnBlock: doc.getElementById("btnBlock"),
    btnRemove: doc.getElementById("removePermission"),
    allowText: win.gPermissionManager._getCapabilityString(
      Ci.nsIPermissionManager.ALLOW_ACTION),
    denyText: win.gPermissionManager._getCapabilityString(
      Ci.nsIPermissionManager.DENY_ACTION),
    allow: Ci.nsIPermissionManager.ALLOW_ACTION,
    deny: Ci.nsIPermissionManager.DENY_ACTION,
  };
  let btnApplyChanges = doc.getElementById("btnApplyChanges");
  let observeAllPromise = createObserveAllPromise(observances);

  await test(params, observeAllPromise, () => btnApplyChanges.doCommand());

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

function createObserveAllPromise(observances) {
  return new Promise(resolve => {
    let permObserver = {
      observe(aSubject, aTopic, aData) {
        if (aTopic != "perm-changed")
          return;

        if (observances.length == 0) {
          // Should fail here as we are not expecting a notification, but we
          // don't. See bug 1063410.
          return;
        }

        info(`observed perm-changed (remaining ${observances.length - 1})`);

        let permission = aSubject.QueryInterface(Ci.nsIPermission);
        let expected = observances.shift();

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

        if (observances.length == 0) {
          Services.obs.removeObserver(permObserver, "perm-changed");
          executeSoon(resolve);
        }
      }
    };
    Services.obs.addObserver(permObserver, "perm-changed");
  });
}
