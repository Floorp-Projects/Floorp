/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

let imports = {};

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm", imports);

imports.XPCOMUtils.defineLazyGetter(imports, "prefBranch", function() {
  let prefService = Components.classes["@mozilla.org/preferences-service;1"]
          .getService(Components.interfaces.nsIPrefService);
  return prefService.getBranch(null)
          .QueryInterface(Components.interfaces.nsIPrefBranch2);
});

imports.XPCOMUtils.defineLazyGetter(imports, "supportsString", function() {
  return Components.classes["@mozilla.org/supports-string;1"]
          .createInstance(Components.interfaces.nsISupportsString);
});

const TEST_URI = "data:text/html;charset=utf-8,gcli-pref";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    setup();

    testPrefSetEnable();
    testPrefStatus();
    testPrefBoolExec();
    testPrefNumberExec();
    testPrefStringExec();
    testPrefSetDisable();

    shutdown();
    finish();
  });
}

let tiltEnabledOrig = undefined;
let tabSizeOrig = undefined;
let remoteHostOrig = undefined;

function setup() {
  Components.utils.import("resource://gre/modules/devtools/Require.jsm", imports);
  imports.settings = imports.require("gcli/settings");

  tiltEnabledOrig = imports.prefBranch.getBoolPref("devtools.tilt.enabled");
  tabSizeOrig = imports.prefBranch.getIntPref("devtools.editor.tabsize");
  remoteHostOrig = imports.prefBranch.getComplexValue(
          "devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString).data;

  info("originally: devtools.tilt.enabled = " + tiltEnabledOrig);
  info("originally: devtools.editor.tabsize = " + tabSizeOrig);
  info("originally: devtools.debugger.remote-host = " + remoteHostOrig);
}

function shutdown() {
  imports.prefBranch.setBoolPref("devtools.tilt.enabled", tiltEnabledOrig);
  imports.prefBranch.setIntPref("devtools.editor.tabsize", tabSizeOrig);
  imports.supportsString.data = remoteHostOrig;
  imports.prefBranch.setComplexValue("devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString,
          imports.supportsString);

  tiltEnabledOrig = undefined;
  tabSizeOrig = undefined;
  remoteHostOrig = undefined;

  imports = undefined;
}

function testPrefStatus() {
  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref s",
    markup: "IIIIVI",
    status: "ERROR",
    directTabText: "et"
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show",
    markup: "VVVVVVVVV",
    status: "ERROR",
    emptyParameters: [ " <setting>" ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show tempTBo",
    markup: "VVVVVVVVVVEEEEEEE",
    status: "ERROR",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show devtools.toolbar.ena",
    markup: "VVVVVVVVVVIIIIIIIIIIIIIIIIIIII",
    directTabText: "bled",
    status: "ERROR",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show hideIntro",
    markup: "VVVVVVVVVVIIIIIIIII",
    directTabText: "",
    arrowTabText: "devtools.gcli.hideIntro",
    status: "ERROR",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show devtools.toolbar.enabled",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show devtools.tilt.enabled 4",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE",
    directTabText: "",
    status: "ERROR",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref show devtools.tilt.enabled",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref reset devtools.tilt.enabled",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref set devtools.tilt.enabled 4",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE",
    status: "ERROR",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref set devtools.editor.tabsize 4",
    markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
    status: "VALID",
    emptyParameters: [ ]
  });

  DeveloperToolbarTest.checkInputStatus({
    typed:  "pref list",
    markup: "EEEEVEEEE",
    status: "ERROR",
    emptyParameters: [ ]
  });
}

function testPrefSetEnable() {
  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 9",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: 9
    },
    completed: true,
    outputMatch: [ /void your warranty/, /I promise/ ],
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"),
          tabSizeOrig,
          "devtools.editor.tabsize is unchanged");

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.gcli.allowSet true",
    args: {
      setting: imports.settings.getSetting("devtools.gcli.allowSet"),
      value: true
    },
    completed: true,
    blankOutput: true,
  });

  is(imports.prefBranch.getBoolPref("devtools.gcli.allowSet"), true,
          "devtools.gcli.allowSet is true");

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 10",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: 10
    },
    completed: true,
    blankOutput: true,
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"),
          10,
          "devtools.editor.tabsize is 10");
}

function testPrefBoolExec() {
  DeveloperToolbarTest.exec({
    typed: "pref show devtools.tilt.enabled",
    args: {
      setting: imports.settings.getSetting("devtools.tilt.enabled")
    },
    completed: true,
    outputMatch: new RegExp("^" + tiltEnabledOrig + "$"),
  });

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.tilt.enabled true",
    args: {
      setting: imports.settings.getSetting("devtools.tilt.enabled"),
      value: true
    },
    completed: true,
    blankOutput: true,
  });

  is(imports.prefBranch.getBoolPref("devtools.tilt.enabled"), true,
          "devtools.tilt.enabled is true");

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.tilt.enabled",
    args: {
      setting: imports.settings.getSetting("devtools.tilt.enabled")
    },
    completed: true,
    outputMatch: new RegExp("^true$"),
  });

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.tilt.enabled false",
    args: {
      setting: imports.settings.getSetting("devtools.tilt.enabled"),
      value: false
    },
    completed: true,
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.tilt.enabled",
    args: {
      setting: imports.settings.getSetting("devtools.tilt.enabled")
    },
    completed: true,
    outputMatch: new RegExp("^false$"),
  });

  is(imports.prefBranch.getBoolPref("devtools.tilt.enabled"), false,
          "devtools.tilt.enabled is false");
}

function testPrefNumberExec() {
  DeveloperToolbarTest.exec({
    typed: "pref show devtools.editor.tabsize",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize")
    },
    completed: true,
    outputMatch: new RegExp("^10$"),
  });

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 20",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: 20
    },
    completed: true,
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.editor.tabsize",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize")
    },
    completed: true,
    outputMatch: new RegExp("^20$"),
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"), 20,
          "devtools.editor.tabsize is 20");

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 1",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: true
    },
    completed: true,
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.editor.tabsize",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize")
    },
    completed: true,
    outputMatch: new RegExp("^1$"),
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"), 1,
          "devtools.editor.tabsize is 1");
}

function testPrefStringExec() {
  DeveloperToolbarTest.exec({
    typed: "pref show devtools.debugger.remote-host",
    args: {
      setting: imports.settings.getSetting("devtools.debugger.remote-host")
    },
    completed: true,
    outputMatch: new RegExp("^" + remoteHostOrig + "$"),
  });

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.debugger.remote-host e.com",
    args: {
      setting: imports.settings.getSetting("devtools.debugger.remote-host"),
      value: "e.com"
    },
    completed: true,
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.debugger.remote-host",
    args: {
      setting: imports.settings.getSetting("devtools.debugger.remote-host")
    },
    completed: true,
    outputMatch: new RegExp("^e.com$"),
  });

  var ecom = imports.prefBranch.getComplexValue(
          "devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString).data;
  is(ecom, "e.com", "devtools.debugger.remote-host is e.com");

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.debugger.remote-host moz.foo",
    args: {
      setting: imports.settings.getSetting("devtools.debugger.remote-host"),
      value: "moz.foo"
    },
    completed: true,
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "pref show devtools.debugger.remote-host",
    args: {
      setting: imports.settings.getSetting("devtools.debugger.remote-host")
    },
    completed: true,
    outputMatch: new RegExp("^moz.foo$"),
  });

  var mozfoo = imports.prefBranch.getComplexValue(
          "devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString).data;
  is(mozfoo, "moz.foo", "devtools.debugger.remote-host is moz.foo");
}

function testPrefSetDisable() {
  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 32",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: 32
    },
    completed: true,
    blankOutput: true,
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"), 32,
          "devtools.editor.tabsize is 32");

  DeveloperToolbarTest.exec({
    typed: "pref reset devtools.gcli.allowSet",
    args: {
      setting: imports.settings.getSetting("devtools.gcli.allowSet")
    },
    completed: true,
    blankOutput: true,
  });

  is(imports.prefBranch.getBoolPref("devtools.gcli.allowSet"), false,
          "devtools.gcli.allowSet is false");

  DeveloperToolbarTest.exec({
    typed: "pref set devtools.editor.tabsize 33",
    args: {
      setting: imports.settings.getSetting("devtools.editor.tabsize"),
      value: 33
    },
    completed: true,
    outputMatch: [ /void your warranty/, /I promise/ ],
  });

  is(imports.prefBranch.getIntPref("devtools.editor.tabsize"), 32,
          "devtools.editor.tabsize is still 32");
}
