// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cm = Components.manager;
const Cc = Components.classes;

const CONTRACT_ID = "@mozilla.org/xre/runtime;1";

var gAppInfoClassID, gIncOldFactory, gOldCrashSubmit, gCrashesSubmitted;
var gMockAppInfoQueried = false;

function MockAppInfo() {
}

var newFactory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new MockAppInfo().QueryInterface(aIID);
  },
  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

function initMockAppInfo() {
  var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
  gAppInfoClassID = registrar.contractIDToCID(CONTRACT_ID);
  gIncOldFactory = Cm.getClassObject(Cc[CONTRACT_ID], Ci.nsIFactory);
  registrar.unregisterFactory(gAppInfoClassID, gIncOldFactory);
  var components = [MockAppInfo];
  registrar.registerFactory(gAppInfoClassID, "", CONTRACT_ID, newFactory);
  gIncOldFactory = Cm.getClassObject(Cc[CONTRACT_ID], Ci.nsIFactory);

  gCrashesSubmitted = 0;
  gOldCrashSubmit = BrowserUI.CrashSubmit;
  BrowserUI.CrashSubmit = {
    submit: function() {
      gCrashesSubmitted++;
    }
  };
}

function cleanupMockAppInfo() {
  if (gIncOldFactory) {
    var registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(gAppInfoClassID, newFactory);
    registrar.registerFactory(gAppInfoClassID, "", CONTRACT_ID, gIncOldFactory);
  }
  gIncOldFactory = null;
  BrowserUI.CrashSubmit = gOldCrashSubmit;
}

MockAppInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULRuntime]),
  get lastRunCrashID() {
    gMockAppInfoQueried = true;
    return this.crashid;
  },
}

function GetAutoSubmitPref() {
  return Services.prefs.getBoolPref("app.crashreporter.autosubmit");
}

function SetAutoSubmitPref(aValue) {
  Services.prefs.setBoolPref('app.crashreporter.autosubmit', aValue);
}

function GetPromptedPref() {
  return Services.prefs.getBoolPref("app.crashreporter.prompted");
}

function SetPromptedPref(aValue) {
  Services.prefs.setBoolPref('app.crashreporter.prompted', aValue);
}

gTests.push({
  desc: "Pressing 'cancel' button on the crash reporter prompt",
  setUp: initMockAppInfo,
  tearDown: cleanupMockAppInfo,

  run: function() {
    MockAppInfo.crashid = "testid";
    SetAutoSubmitPref(false);
    SetPromptedPref(false);

    BrowserUI.startupCrashCheck();
    yield waitForCondition2(function () {
      return Browser.selectedBrowser.currentURI.spec == "about:crashprompt";
    }, "Loading crash prompt");

    yield Browser.selectedTab.pageShowPromise;
    ok(true, "Loaded crash prompt");

    EventUtils.sendMouseEvent({type: "click"},
                              "refuseButton",
                              Browser.selectedBrowser
                                     .contentDocument
                                     .defaultView);
    yield waitForCondition2(function () {
      return Browser.selectedBrowser.currentURI.spec == "about:blank";
    }, "Crash prompt dismissed");

    ok(!GetAutoSubmitPref(), "auto-submit pref should still be false");
    ok(GetPromptedPref(), "prompted pref should now be true");
    is(gCrashesSubmitted, 0, "did not submit crash report");
  }
});

gTests.push({
  desc: "Pressing 'Send Reports' button on the crash reporter prompt",
  setUp: initMockAppInfo,
  tearDown: cleanupMockAppInfo,

  run: function() {
    MockAppInfo.crashid = "testid";
    SetAutoSubmitPref(false);
    SetPromptedPref(false);

    BrowserUI.startupCrashCheck();
    yield waitForCondition2(function () {
      return Browser.selectedBrowser.currentURI.spec == "about:crashprompt";
    }, "Loading crash prompt");

    yield Browser.selectedTab.pageShowPromise;
    ok(true, "Loaded crash prompt");

    EventUtils.sendMouseEvent({type: "click"},
                              "sendReportsButton",
                              Browser.selectedBrowser
                                     .contentDocument
                                     .defaultView);
    yield waitForCondition2(function () {
      return Browser.selectedBrowser.currentURI.spec == "about:blank";
    }, "Crash prompt dismissed");

    ok(GetAutoSubmitPref(), "auto-submit pref should now be true");
    ok(GetPromptedPref(), "prompted pref should now be true");
    // TODO: We don't submit a crash report when the user selects
    // 'Send Reports' but eventually we want to.
    // is(gCrashesSubmitted, 1, "submitted 1 crash report");
  }
});

gTests.push({
  desc: "Already prompted, crash reporting disabled",
  setUp: initMockAppInfo,
  tearDown: cleanupMockAppInfo,

  run: function() {
    MockAppInfo.crashid = "testid";
    SetAutoSubmitPref(false);
    SetPromptedPref(true);

    BrowserUI.startupCrashCheck();
    is(Browser.selectedBrowser.currentURI.spec,
       "about:blank",
       "Not loading crash prompt");

    ok(!GetAutoSubmitPref(), "auto-submit pref should still be false");
    ok(GetPromptedPref(), "prompted pref should still be true");
    is(gCrashesSubmitted, 0, "did not submit crash report");
  }
});

gTests.push({
  desc: "Already prompted, crash reporting enabled",
  setUp: initMockAppInfo,
  tearDown: cleanupMockAppInfo,

  run: function() {
    MockAppInfo.crashid = "testid";
    SetAutoSubmitPref(true);
    SetPromptedPref(true);

    BrowserUI.startupCrashCheck();
    is(Browser.selectedBrowser.currentURI.spec,
       "about:blank",
       "Not loading crash prompt");

    ok(GetAutoSubmitPref(), "auto-submit pref should still be true");
    ok(GetPromptedPref(), "prompted pref should still be true");
    is(gCrashesSubmitted, 1, "submitted 1 crash report");
  }
});

gTests.push({
  desc: "Crash reporting enabled, not prompted",
  setUp: initMockAppInfo,
  tearDown: cleanupMockAppInfo,

  run: function() {
    MockAppInfo.crashid = "testid";
    SetAutoSubmitPref(true);
    SetPromptedPref(false);

    BrowserUI.startupCrashCheck();
    is(Browser.selectedBrowser.currentURI.spec,
       "about:blank",
       "Not loading crash prompt");

    ok(GetAutoSubmitPref(), "auto-submit pref should still be true");
    // We don't check the "prompted" pref; it's equally correct for
    // the pref to be true or false at this point
    is(gCrashesSubmitted, 1, "submitted 1 crash report");
  }
});

function test() {
  if (!CrashReporter.enabled) {
    info("crash reporter prompt tests didn't run, CrashReporter.enabled is false.");
    return;
  }
  runTests();
}
