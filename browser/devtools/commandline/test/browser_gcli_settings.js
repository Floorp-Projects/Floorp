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

const TEST_URI = "data:text/html;charset=utf-8,gcli-settings";

function test() {
  DeveloperToolbarTest.test(TEST_URI, function(browser, tab) {
    setup();

    testSettings();

    shutdown();
    finish();
  });
}

let tiltEnabled = undefined;
let tabSize = undefined;
let remoteHost = undefined;

let tiltEnabledOrig = undefined;
let tabSizeOrig = undefined;
let remoteHostOrig = undefined;

function setup() {
  Components.utils.import("resource:///modules/devtools/Require.jsm", imports);
  imports.settings = imports.require("gcli/settings");

  tiltEnabled = imports.settings.getSetting("devtools.tilt.enabled");
  tabSize = imports.settings.getSetting("devtools.editor.tabsize");
  remoteHost = imports.settings.getSetting("devtools.debugger.remote-host");

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

  tiltEnabled = undefined;
  tabSize = undefined;
  remoteHost = undefined;

  tiltEnabledOrig = undefined;
  tabSizeOrig = undefined;
  remoteHostOrig = undefined;

  imports = undefined;
}

function testSettings() {
  is(tiltEnabled.value, tiltEnabledOrig, "tiltEnabled default");
  is(tabSize.value, tabSizeOrig, "tabSize default");
  is(remoteHost.value, remoteHostOrig, "remoteHost default");

  tiltEnabled.setDefault();
  tabSize.setDefault();
  remoteHost.setDefault();

  let tiltEnabledDefault = tiltEnabled.value;
  let tabSizeDefault = tabSize.value;
  let remoteHostDefault = remoteHost.value;

  tiltEnabled.value = false;
  tabSize.value = 42;
  remoteHost.value = "example.com"

  is(tiltEnabled.value, false, "tiltEnabled basic");
  is(tabSize.value, 42, "tabSize basic");
  is(remoteHost.value, "example.com", "remoteHost basic");

  function tiltEnabledCheck(ev) {
    is(ev.setting, tiltEnabled, "tiltEnabled event setting");
    is(ev.value, true, "tiltEnabled event value");
    is(ev.setting.value, true, "tiltEnabled event setting value");
  }
  tiltEnabled.onChange.add(tiltEnabledCheck);
  tiltEnabled.value = true;
  is(tiltEnabled.value, true, "tiltEnabled change");

  function tabSizeCheck(ev) {
    is(ev.setting, tabSize, "tabSize event setting");
    is(ev.value, 1, "tabSize event value");
    is(ev.setting.value, 1, "tabSize event setting value");
  }
  tabSize.onChange.add(tabSizeCheck);
  tabSize.value = 1;
  is(tabSize.value, 1, "tabSize change");

  function remoteHostCheck(ev) {
    is(ev.setting, remoteHost, "remoteHost event setting");
    is(ev.value, "y.com", "remoteHost event value");
    is(ev.setting.value, "y.com", "remoteHost event setting value");
  }
  remoteHost.onChange.add(remoteHostCheck);
  remoteHost.value = "y.com";
  is(remoteHost.value, "y.com", "remoteHost change");

  tiltEnabled.onChange.remove(tiltEnabledCheck);
  tabSize.onChange.remove(tabSizeCheck);
  remoteHost.onChange.remove(remoteHostCheck);

  function remoteHostReCheck(ev) {
    is(ev.setting, remoteHost, "remoteHost event reset");
    is(ev.value, null, "remoteHost event revalue");
    is(ev.setting.value, null, "remoteHost event setting revalue");
  }
  remoteHost.onChange.add(remoteHostReCheck);

  tiltEnabled.setDefault();
  tabSize.setDefault();
  remoteHost.setDefault();

  remoteHost.onChange.remove(remoteHostReCheck);

  is(tiltEnabled.value, tiltEnabledDefault, "tiltEnabled reset");
  is(tabSize.value, tabSizeDefault, "tabSize reset");
  is(remoteHost.value, remoteHostDefault, "remoteHost reset");
}
