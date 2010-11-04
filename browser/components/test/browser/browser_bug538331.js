/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
                    "extensionVersion=\"1.0\" installDate=\"1238441400314\" " +
                    "isCompleteUpdate=\"true\" name=\"Update Test 1.0\" " +
                    "serviceURL=\"https://example.com/\" showNeverForVersion=" +
                    "\"false\" showPrompt=\"false\" showSurvey=\"false\" type=" +
                    "\"minor\" version=\"version 1.0\" detailsURL=" +
                    "\"http://example.com/\" previousAppVersion=\"1.0\" " +
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

__defineGetter__("gBG", function() {
  delete this.gBG;
  return this.gBG = Cc["@mozilla.org/browser/browserglue;1"].
                    getService(Ci.nsIBrowserGlue).
                    QueryInterface(Ci.nsIObserver);
});

function test()
{
  waitForExplicitFinish();

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
  start: function() {
    Services.ww.registerNotification(this);
  },

  finish: function(aFunc) {
    Services.ww.unregisterNotification(this);
    this.finishFunc = aFunc;
    if (this.windowsOpen > 0)
      return;

    this.finishFunc();
  },

  closeWindow: function (win) {
    info("window catcher closing window: " + win.document.documentURI);
    win.close();
    this.windowsOpen--;
    if (this.finishFunc) {
      this.finish(this.finishFunc);
    }
  },

  windowLoad: function (win) {
    executeSoon(this.closeWindow.bind(this, win));
  },

  observe: function(subject, topic, data) {
    if (topic != "domwindowopened")
      return;

    this.windowsOpen++;
    let win = subject.QueryInterface(Ci.nsIDOMWindow);
    info("window catcher caught window opening: " + win.document.documentURI);
    win.addEventListener("load", this.windowLoad.bind(this, win), false);
  }
};

function finish_test()
{
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
function testDefaultArgs()
{
  // Clear any pre-existing override in defaultArgs that are hanging around.
  // This will also set the browser.startup.homepage_override.mstone preference
  // if it isn't already set.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  let originalMstone = gPrefService.getCharPref(PREF_MSTONE);

  gPrefService.setCharPref(PREF_OVERRIDE_URL, DEFAULT_PREF_URL);

  writeUpdatesToXMLFile(XML_EMPTY);
  reloadUpdateManagerData();

  for (let i = 0; i < BCH_TESTS.length; i++) {
    let test = BCH_TESTS[i];
    ok(true, "Test nsBrowserContentHandler " + (i + 1) + ": " + test.description);

    if (test.actions) {
      let actionsXML = " actions=\"" + test.actions + "\"";
      if (test.openURL) {
        actionsXML += " openURL=\"" + test.openURL + "\"";
      }
      writeUpdatesToXMLFile(XML_PREFIX + actionsXML + XML_SUFFIX);
    } else {
      writeUpdatesToXMLFile(XML_EMPTY);
    }

    reloadUpdateManagerData();

    let noOverrideArgs = Cc["@mozilla.org/browser/clh;1"].
                         getService(Ci.nsIBrowserHandler).defaultArgs;

    let overrideArgs = "";
    if (test.prefURL) {
      overrideArgs = test.prefURL;
    } else if (test.openURL) {
      overrideArgs = test.openURL;
    }

    if (overrideArgs == "" && noOverrideArgs) {
      overrideArgs = noOverrideArgs;
    } else if (noOverrideArgs) {
      overrideArgs += "|" + noOverrideArgs;
    }

    if (test.noMstoneChange === undefined) {
      gPrefService.setCharPref(PREF_MSTONE, "PreviousMilestone");
    }

    if (test.noPostUpdatePref == undefined) {
      gPrefService.setBoolPref(PREF_POSTUPDATE, true);
    }

    let defaultArgs = Cc["@mozilla.org/browser/clh;1"].
                      getService(Ci.nsIBrowserHandler).defaultArgs;
    is(defaultArgs, overrideArgs, "correct value returned by defaultArgs");

    if (test.noMstoneChange === undefined || test.noMstoneChange != true) {
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
function testShowNotification()
{
  let gTestBrowser = gBrowser.selectedBrowser;
  let notifyBox = gBrowser.getNotificationBox(gTestBrowser);

  // Catches any windows opened by these tests (e.g. alert windows) and closes
  // them
  gWindowCatcher.start();

  for (let i = 0; i < BG_NOTIFY_TESTS.length; i++) {
    let test = BG_NOTIFY_TESTS[i];
    ok(true, "Test showNotification " + (i + 1) + ": " + test.description);

    if (test.actions) {
      let actionsXML = " actions=\"" + test.actions + "\"";
      if (test.notificationText) {
        actionsXML += " notificationText=\"" + test.notificationText + "\"";
      }
      if (test.notificationURL) {
        actionsXML += " notificationURL=\"" + test.notificationURL + "\"";
      }
      if (test.notificationButtonLabel) {
        actionsXML += " notificationButtonLabel=\"" + test.notificationButtonLabel + "\"";
      }
      if (test.notificationButtonAccessKey) {
        actionsXML += " notificationButtonAccessKey=\"" + test.notificationButtonAccessKey + "\"";
      }
      writeUpdatesToXMLFile(XML_PREFIX + actionsXML + XML_SUFFIX);
    } else {
      writeUpdatesToXMLFile(XML_EMPTY);
    }

    reloadUpdateManagerData();
    gPrefService.setBoolPref(PREF_POSTUPDATE, true);

    gBG.observe(null, "browser-glue-test", "post-update-notification");

    let updateBox = notifyBox.getNotificationWithValue("post-update-notification");
    if (test.actions && test.actions.indexOf("showNotification") != -1 &&
        test.actions.indexOf("silent") == -1) {
      ok(updateBox, "Update notification box should have been displayed");
      if (updateBox) {
        if (test.notificationText) {
          is(updateBox.label, test.notificationText, "Update notification box " +
             "should have the label provided by the update");
        }
        if (test.notificationButtonLabel) {
          var button = updateBox.getElementsByTagName("button").item(0);
          is(button.label, test.notificationButtonLabel, "Update notification " +
             "box button should have the label provided by the update");
          if (test.notificationButtonAccessKey) {
            let accessKey = button.getAttribute("accesskey");
            is(accessKey, test.notificationButtonAccessKey, "Update " +
               "notification box button should have the accesskey " +
               "provided by the update");
          }
        }
        // The last test opens an url and verifies the url from the updates.xml
        // is correct.
        if (i == (BG_NOTIFY_TESTS.length - 1)) {
          // Wait for any windows caught by the windowcatcher to close
          gWindowCatcher.finish(function () {
            button.click();
            gBrowser.selectedBrowser.addEventListener("load", testNotificationURL, true);
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
function testNotificationURL()
{
  ok(true, "Test testNotificationURL: clicking the notification button " +
           "opened the url specified by the update");
  let href = gBrowser.selectedBrowser.contentWindow.location.href;
  let expectedURL = BG_NOTIFY_TESTS[BG_NOTIFY_TESTS.length - 1].notificationURL;
  is(href, expectedURL, "The url opened from the notification should be the " +
     "url provided by the update");
  gBrowser.removeCurrentTab();
  window.focus();
  finish_test();
}

/* Reloads the update metadata from disk */
function reloadUpdateManagerData()
{
  Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager).
  QueryInterface(Ci.nsIObserver).observe(null, "um-reload-update-data", "");
}


function writeUpdatesToXMLFile(aText)
{
  const PERMS_FILE = 0644;

  const MODE_WRONLY   = 0x02;
  const MODE_CREATE   = 0x08;
  const MODE_TRUNCATE = 0x20;

  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get("XCurProcD", Ci.nsIFile);
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
