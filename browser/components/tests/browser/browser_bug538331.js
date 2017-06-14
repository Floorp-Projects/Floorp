/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const PREF_POSTUPDATE = "app.update.postupdate";
const PREF_MSTONE = "browser.startup.homepage_override.mstone";
const PREF_OVERRIDE_URL = "startup.homepage_override_url";

const DEFAULT_PREF_URL = "http://pref.example.com/";
const DEFAULT_UPDATE_URL = "http://example.com/";

const XML_EMPTY = "<?xml version=\"1.0\"?><updates xmlns=" +
                  "\"http://www.mozilla.org/2005/app-update\"></updates>";

const XML_PREFIX =  "<updates xmlns=\"http://www.mozilla.org/2005/app-update\"" +
                    "><update appVersion=\"1.0\" buildID=\"20080811053724\" " +
                    "channel=\"nightly\" displayVersion=\"Version 1.0\" " +
                    "installDate=\"1238441400314\" isCompleteUpdate=\"true\" " +
                    "name=\"Update Test 1.0\" type=\"minor\" detailsURL=" +
                    "\"http://example.com/\" previousAppVersion=\"1.0\" " +
                    "serviceURL=\"https://example.com/\" " +
                    "statusText=\"The Update was successfully installed\" " +
                    "foregroundDownload=\"true\"";

const XML_SUFFIX = "><patch type=\"complete\" URL=\"http://example.com/\" " +
                   "hashFunction=\"MD5\" hashValue=" +
                   "\"6232cd43a1c77e30191c53a329a3f99d\" size=\"775\" " +
                   "selected=\"true\" state=\"succeeded\"/></update></updates>";

// nsBrowserContentHandler.js defaultArgs tests
const BCH_TESTS = [
  {
    description: "no mstone change and no update",
    noPostUpdatePref: true,
    noMstoneChange: true
  }, {
    description: "mstone changed and no update",
    noPostUpdatePref: true,
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "no mstone change and update with 'showURL' for actions",
    actions: "showURL",
    noMstoneChange: true
  }, {
    description: "update without actions",
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "update with 'showURL' for actions",
    actions: "showURL",
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "update with 'showURL' for actions and openURL",
    actions: "showURL",
    openURL: DEFAULT_UPDATE_URL
    }, {
    description: "update with 'showURL showAlert' for actions",
    actions: "showAlert showURL",
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "update with 'showAlert showURL' for actions and openURL",
    actions: "showAlert showURL",
    openURL: DEFAULT_UPDATE_URL
  }, {
    description: "update with 'showURL showNotification' for actions",
    actions: "showURL showNotification",
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "update with 'showNotification showURL' for actions and " +
                 "openURL",
    actions: "showNotification showURL",
    openURL: DEFAULT_UPDATE_URL
  }, {
    description: "update with 'showAlert showURL showNotification' for actions",
    actions: "showAlert showURL showNotification",
    prefURL: DEFAULT_PREF_URL
  }, {
    description: "update with 'showNotification showURL showAlert' for " +
                 "actions and openURL",
    actions: "showNotification showURL showAlert",
    openURL: DEFAULT_UPDATE_URL
  }, {
    description: "update with 'showAlert' for actions",
    actions: "showAlert"
  }, {
    description: "update with 'showAlert showNotification' for actions",
    actions: "showAlert showNotification"
  }, {
    description: "update with 'showNotification' for actions",
    actions: "showNotification"
  }, {
    description: "update with 'showNotification showAlert' for actions",
    actions: "showNotification showAlert"
  }, {
    description: "update with 'silent' for actions",
    actions: "silent"
  }, {
    description: "update with 'silent showURL showAlert showNotification' " +
                 "for actions and openURL",
    actions: "silent showURL showAlert showNotification"
  }
];

var gOriginalMStone;
var gOriginalOverrideURL;

this.__defineGetter__("gBG", function() {
  delete this.gBG;
  return this.gBG = Cc["@mozilla.org/browser/browserglue;1"].
                    getService(Ci.nsIObserver);
});

function test() {
  waitForExplicitFinish();

  // Reset the startup page pref since it may have been set by other tests
  // and we will assume it is default.
  Services.prefs.clearUserPref("browser.startup.page");

  if (gPrefService.prefHasUserValue(PREF_MSTONE)) {
    gOriginalMStone = gPrefService.getCharPref(PREF_MSTONE);
  }

  if (gPrefService.prefHasUserValue(PREF_OVERRIDE_URL)) {
    gOriginalOverrideURL = gPrefService.getCharPref(PREF_OVERRIDE_URL);
  }

  testDefaultArgs();
}

var gWindowCatcher = {
  windowsOpen: 0,
  finishCalled: false,
  start() {
    Services.ww.registerNotification(this);
  },

  finish(aFunc) {
    Services.ww.unregisterNotification(this);
    this.finishFunc = aFunc;
    if (this.windowsOpen > 0)
      return;

    this.finishFunc();
  },

  closeWindow(win) {
    info("window catcher closing window: " + win.document.documentURI);
    win.close();
    this.windowsOpen--;
    if (this.finishFunc) {
      this.finish(this.finishFunc);
    }
  },

  windowLoad(win) {
    executeSoon(this.closeWindow.bind(this, win));
  },

  observe(subject, topic, data) {
    if (topic != "domwindowopened")
      return;

    this.windowsOpen++;
    let win = subject.QueryInterface(Ci.nsIDOMWindow);
    info("window catcher caught window opening: " + win.document.documentURI);
    win.addEventListener("load", function() {
      gWindowCatcher.windowLoad(win);
    }, {once: true});
  }
};

function finish_test() {
  // Reset browser.startup.homepage_override.mstone to the original value or
  // clear it if it didn't exist.
  if (gOriginalMStone) {
    gPrefService.setCharPref(PREF_MSTONE, gOriginalMStone);
  } else if (gPrefService.prefHasUserValue(PREF_MSTONE)) {
    gPrefService.clearUserPref(PREF_MSTONE);
  }

  // Reset startup.homepage_override_url to the original value or clear it if
  // it didn't exist.
  if (gOriginalOverrideURL) {
    gPrefService.setCharPref(PREF_OVERRIDE_URL, gOriginalOverrideURL);
  } else if (gPrefService.prefHasUserValue(PREF_OVERRIDE_URL)) {
    gPrefService.clearUserPref(PREF_OVERRIDE_URL);
  }

  writeUpdatesToXMLFile(XML_EMPTY);
  reloadUpdateManagerData();

  finish();
}

// Test the defaultArgs returned by nsBrowserContentHandler after an update
function testDefaultArgs() {
  // Clear any pre-existing override in defaultArgs that are hanging around.
  // This will also set the browser.startup.homepage_override.mstone preference
  // if it isn't already set.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  let originalMstone = gPrefService.getCharPref(PREF_MSTONE);

  gPrefService.setCharPref(PREF_OVERRIDE_URL, DEFAULT_PREF_URL);

  writeUpdatesToXMLFile(XML_EMPTY);
  reloadUpdateManagerData();

  for (let i = 0; i < BCH_TESTS.length; i++) {
    let testCase = BCH_TESTS[i];
    ok(true, "Test nsBrowserContentHandler " + (i + 1) + ": " + testCase.description);

    if (testCase.actions) {
      let actionsXML = " actions=\"" + testCase.actions + "\"";
      if (testCase.openURL) {
        actionsXML += " openURL=\"" + testCase.openURL + "\"";
      }
      writeUpdatesToXMLFile(XML_PREFIX + actionsXML + XML_SUFFIX);
    } else {
      writeUpdatesToXMLFile(XML_EMPTY);
    }

    reloadUpdateManagerData();

    let noOverrideArgs = Cc["@mozilla.org/browser/clh;1"].
                         getService(Ci.nsIBrowserHandler).defaultArgs;

    let overrideArgs = "";
    if (testCase.prefURL) {
      overrideArgs = testCase.prefURL;
    } else if (testCase.openURL) {
      overrideArgs = testCase.openURL;
    }

    if (overrideArgs == "" && noOverrideArgs) {
      overrideArgs = noOverrideArgs;
    } else if (noOverrideArgs) {
      overrideArgs += "|" + noOverrideArgs;
    }

    if (testCase.noMstoneChange === undefined) {
      gPrefService.setCharPref(PREF_MSTONE, "PreviousMilestone");
    }

    if (testCase.noPostUpdatePref == undefined) {
      gPrefService.setBoolPref(PREF_POSTUPDATE, true);
    }

    let defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                      getService(Ci.nsIBrowserHandler).defaultArgs;
    is(defaultArgs, overrideArgs, "correct value returned by defaultArgs");

    if (testCase.noMstoneChange === undefined || testCase.noMstoneChange != true) {
      let newMstone = gPrefService.getCharPref(PREF_MSTONE);
      is(originalMstone, newMstone, "preference " + PREF_MSTONE +
         " should have been updated");
    }

    if (gPrefService.prefHasUserValue(PREF_POSTUPDATE)) {
      gPrefService.clearUserPref(PREF_POSTUPDATE);
    }
  }

  testShowNotification();
}

// nsBrowserGlue.js _showUpdateNotification notification tests
const BG_NOTIFY_TESTS = [
  {
    description: "'silent showNotification' actions should not display a notification",
    actions: "silent showNotification"
  }, {
    description: "'showNotification' for actions should display a notification",
    actions: "showNotification"
  }, {
    description: "no actions and empty updates.xml",
  }, {
    description: "'showAlert' for actions should not display a notification",
    actions: "showAlert"
  }, {
    // This test MUST be the last test in the array to test opening the url
    // provided by the updates.xml.
    description: "'showNotification' for actions with custom notification " +
                 "attributes should display a notification",
    actions: "showNotification",
    notificationText: "notification text",
    notificationURL: DEFAULT_UPDATE_URL,
    notificationButtonLabel: "button label",
    notificationButtonAccessKey: "b"
  }
];

// Test showing a notification after an update
// _showUpdateNotification in nsBrowserGlue.js
function testShowNotification() {
  let notifyBox = document.getElementById("high-priority-global-notificationbox");

  // Catches any windows opened by these tests (e.g. alert windows) and closes
  // them
  gWindowCatcher.start();

  for (let i = 0; i < BG_NOTIFY_TESTS.length; i++) {
    let testCase = BG_NOTIFY_TESTS[i];
    ok(true, "Test showNotification " + (i + 1) + ": " + testCase.description);

    if (testCase.actions) {
      let actionsXML = " actions=\"" + testCase.actions + "\"";
      if (testCase.notificationText) {
        actionsXML += " notificationText=\"" + testCase.notificationText + "\"";
      }
      if (testCase.notificationURL) {
        actionsXML += " notificationURL=\"" + testCase.notificationURL + "\"";
      }
      if (testCase.notificationButtonLabel) {
        actionsXML += " notificationButtonLabel=\"" + testCase.notificationButtonLabel + "\"";
      }
      if (testCase.notificationButtonAccessKey) {
        actionsXML += " notificationButtonAccessKey=\"" + testCase.notificationButtonAccessKey + "\"";
      }
      writeUpdatesToXMLFile(XML_PREFIX + actionsXML + XML_SUFFIX);
    } else {
      writeUpdatesToXMLFile(XML_EMPTY);
    }

    reloadUpdateManagerData();
    gPrefService.setBoolPref(PREF_POSTUPDATE, true);

    gBG.observe(null, "browser-glue-test", "post-update-notification");

    let updateBox = notifyBox.getNotificationWithValue("post-update-notification");
    if (testCase.actions && testCase.actions.indexOf("showNotification") != -1 &&
        testCase.actions.indexOf("silent") == -1) {
      ok(updateBox, "Update notification box should have been displayed");
      if (updateBox) {
        if (testCase.notificationText) {
          is(updateBox.label, testCase.notificationText, "Update notification box " +
             "should have the label provided by the update");
        }
        if (testCase.notificationButtonLabel) {
          var button = updateBox.getElementsByTagName("button").item(0);
          is(button.label, testCase.notificationButtonLabel, "Update notification " +
             "box button should have the label provided by the update");
          if (testCase.notificationButtonAccessKey) {
            let accessKey = button.getAttribute("accesskey");
            is(accessKey, testCase.notificationButtonAccessKey, "Update " +
               "notification box button should have the accesskey " +
               "provided by the update");
          }
        }
        // The last test opens an url and verifies the url from the updates.xml
        // is correct.
        if (i == (BG_NOTIFY_TESTS.length - 1)) {
          // Wait for any windows caught by the windowcatcher to close
          gWindowCatcher.finish(function() {
            BrowserTestUtils.waitForNewTab(gBrowser).then(testNotificationURL);
            button.click();
          });
        } else {
          notifyBox.removeAllNotifications(true);
        }
      } else if (i == (BG_NOTIFY_TESTS.length - 1)) {
        // If updateBox is null the test has already reported errors so bail
        finish_test();
      }
    } else {
      ok(!updateBox, "Update notification box should not have been displayed");
    }

    let prefHasUserValue = gPrefService.prefHasUserValue(PREF_POSTUPDATE);
    is(prefHasUserValue, false, "preference " + PREF_POSTUPDATE +
       " shouldn't have a user value");
  }
}

// Test opening the url provided by the updates.xml in the last test
function testNotificationURL() {
  ok(true, "Test testNotificationURL: clicking the notification button " +
           "opened the url specified by the update");
  let href = gBrowser.currentURI.spec;
  let expectedURL = BG_NOTIFY_TESTS[BG_NOTIFY_TESTS.length - 1].notificationURL;
  is(href, expectedURL, "The url opened from the notification should be the " +
     "url provided by the update");
  gBrowser.removeCurrentTab();
  window.focus();
  finish_test();
}

/* Reloads the update metadata from disk */
function reloadUpdateManagerData() {
  Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager).
  QueryInterface(Ci.nsIObserver).observe(null, "um-reload-update-data", "");
}


function writeUpdatesToXMLFile(aText) {
  const PERMS_FILE = 0o644;

  const MODE_WRONLY   = 0x02;
  const MODE_CREATE   = 0x08;
  const MODE_TRUNCATE = 0x20;

  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("UpdRootD", Ci.nsIFile);
  file.append("updates.xml");
  let fos = Cc["@mozilla.org/network/file-output-stream;1"].
            createInstance(Ci.nsIFileOutputStream);
  if (!file.exists()) {
    file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }
  fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  fos.close();
}
