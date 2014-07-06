/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  function prefWindowObserver(subject, topic, data) {
    if (topic != "domwindowopened")
      return;

    Services.ww.unregisterNotification(this);

    let win = subject.QueryInterface(Ci.nsIDOMEventTarget);

    win.addEventListener("load", function(event) {
      let historyMode = event.target.getElementById("historyMode");
      historyMode.value = "custom";
      historyMode.doCommand();
      Services.ww.registerNotification(cookiesWindowObserver);
      event.target.getElementById("cookieExceptions").doCommand();
    }, false);
  }

  function cookiesWindowObserver(subject, topic, data) {
    if (topic != "domwindowopened")
      return;

    Services.ww.unregisterNotification(this);

    let win = subject.QueryInterface(Ci.nsIDOMEventTarget);

    win.addEventListener("load", function(event) {
      SimpleTest.executeSoon(function() windowLoad(event, win, dialog));
    }, false);
  }

  Services.ww.registerNotification(prefWindowObserver);

  let dialog = openDialog("chrome://browser/content/preferences/preferences.xul",
                 "Preferences", "chrome,titlebar,toolbar,centerscreen,dialog=no",
                 "panePrivacy");
}

function windowLoad(event, win, dialog) {
  let doc = event.target;
  let tree = doc.getElementById("permissionsTree");
  let statusCol = tree.treeBoxObject.columns.getColumnAt(1);
  let url = doc.getElementById("url");
  let btnAllow = doc.getElementById("btnAllow");
  let btnBlock = doc.getElementById("btnBlock");
  let btnRemove = doc.getElementById("removePermission");
  let pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);
  const allowText = win.gPermissionManager._getCapabilityString(
    Ci.nsIPermissionManager.ALLOW_ACTION);
  const denyText = win.gPermissionManager._getCapabilityString(
    Ci.nsIPermissionManager.DENY_ACTION);
  const allow = Ci.nsIPermissionManager.ALLOW_ACTION;
  const deny = Ci.nsIPermissionManager.DENY_ACTION;

  is(tree.view.rowCount, 0, "no cookie exceptions");

  let tests = [
    {
      test: function() {
        url.value = "test.com";
        btnAllow.doCommand();
        is(tree.view.rowCount, 1, "added exception shows up in treeview");
        is(tree.view.getCellText(0, statusCol), allowText,
           "permission text should be set correctly");
      },
      observances: [{ type: "cookie", host: "test.com", data: "added",
                      capability: allow }]
    },
    {
      test: function() {
        url.value = "test.com";
        btnBlock.doCommand();
        is(tree.view.getCellText(0, statusCol), denyText,
           "permission should change to deny in UI");
      },
      observances: [{ type: "cookie", host: "test.com", data: "changed",
                      capability: deny }],
    },
    {
      test: function() {
        url.value = "test.com";
        btnAllow.doCommand();
        is(tree.view.getCellText(0, statusCol), allowText,
           "permission should revert back to allow");
      },
      observances: [{ type: "cookie", host: "test.com", data: "changed",
                      capability: allow }],
    },
    {
      test: function() {
        url.value = "test.com";
        btnRemove.doCommand();
        is(tree.view.rowCount, 0, "exception should be removed");
      },
      observances: [{ type: "cookie", host: "test.com", data: "deleted" }],
    },
    {
      test: function() {
        let uri = ioService.newURI("http://test.com", null, null);
        pm.add(uri, "popup", Ci.nsIPermissionManager.DENY_ACTION);
        is(tree.view.rowCount, 0, "adding unrelated permission should not change display");
      },
      observances: [{ type: "popup", host: "test.com", data: "added",
                      capability: deny }],
      cleanUp: function() {
        pm.remove("test.com", "popup");
      },
    },
    {
      test: function() {
        url.value = "test.com";
        btnAllow.doCommand();
        pm.remove("test.com", "cookie");
        is(tree.view.rowCount, 0, "display should update when cookie permission is deleted");
      },
      observances: [{ type: "cookie", host: "test.com", data: "added",
                      capability: allow },
                    { type: "cookie", host: "test.com", data: "deleted" }]
    },
  ];

  let permObserver = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic != "perm-changed")
        return;

      if (tests[currentTest].observances.length == 0) {
        // Should fail here as we are not expecting a notification.
      }

      let permission = aSubject.QueryInterface(Ci.nsIPermission);
      let expected = tests[currentTest].observances.shift();

      is(aData, expected.data, "type of message should be the same");
      for each (let prop in ["type", "host", "capability"]) {
        if (expected[prop])
          is(permission[prop], expected[prop],
             "property: \"" + prop  + "\" should be equal");
      }

      if (tests[currentTest].observances.length == 0) {
        SimpleTest.executeSoon(function() {
          if (tests[currentTest].cleanUp)
            tests[currentTest].cleanUp();

          runNextTest();
        });
      }
    },
  };

  let os = Cc["@mozilla.org/observer-service;1"]
             .getService(Ci.nsIObserverService);

  os.addObserver(permObserver, "perm-changed", false);

  var currentTest = -1;
  function runNextTest() {
    currentTest++;
    if (currentTest == tests.length) {
      os.removeObserver(permObserver, "perm-changed");
      win.close();
      dialog.close();
      finish();
      return;
    }

    info("Running test #" + (currentTest + 1) + "\n");
    tests[currentTest].test();
    if (!tests[currentTest].observances)
      runNextTest();
  }

  runNextTest();
}
