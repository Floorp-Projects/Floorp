/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

var prefBranch = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService).getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);

var supportsString = Cc["@mozilla.org/supports-string;1"]
                      .createInstance(Ci.nsISupportsString);

const TEST_URI = "data:text/html;charset=utf-8,gcli-settings";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  // Setup
  let options = yield helpers.openTab(TEST_URI);

  const { createSystem } = require("gcli/system");
  const system = createSystem({ location: "server" });

  const gcliInit = require("devtools/toolkit/gcli/commands/index");
  gcliInit.addAllItemsByModule(system);
  yield system.load();

  let settings = system.settings;

  let hideIntroEnabled = settings.get("devtools.gcli.hideIntro");
  let tabSize = settings.get("devtools.editor.tabsize");
  let remoteHost = settings.get("devtools.debugger.remote-host");

  let hideIntroOrig = prefBranch.getBoolPref("devtools.gcli.hideIntro");
  let tabSizeOrig = prefBranch.getIntPref("devtools.editor.tabsize");
  let remoteHostOrig = prefBranch.getComplexValue(
          "devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString).data;

  info("originally: devtools.gcli.hideIntro = " + hideIntroOrig);
  info("originally: devtools.editor.tabsize = " + tabSizeOrig);
  info("originally: devtools.debugger.remote-host = " + remoteHostOrig);

  // Actual tests
  is(hideIntroEnabled.value, hideIntroOrig, "hideIntroEnabled default");
  is(tabSize.value, tabSizeOrig, "tabSize default");
  is(remoteHost.value, remoteHostOrig, "remoteHost default");

  hideIntroEnabled.setDefault();
  tabSize.setDefault();
  remoteHost.setDefault();

  let hideIntroEnabledDefault = hideIntroEnabled.value;
  let tabSizeDefault = tabSize.value;
  let remoteHostDefault = remoteHost.value;

  hideIntroEnabled.value = false;
  tabSize.value = 42;
  remoteHost.value = "example.com";

  is(hideIntroEnabled.value, false, "hideIntroEnabled basic");
  is(tabSize.value, 42, "tabSize basic");
  is(remoteHost.value, "example.com", "remoteHost basic");

  function hideIntroEnabledCheck(ev) {
    is(ev.setting, hideIntroEnabled, "hideIntroEnabled event setting");
    is(ev.value, true, "hideIntroEnabled event value");
    is(ev.setting.value, true, "hideIntroEnabled event setting value");
  }
  hideIntroEnabled.onChange.add(hideIntroEnabledCheck);
  hideIntroEnabled.value = true;
  is(hideIntroEnabled.value, true, "hideIntroEnabled change");

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

  hideIntroEnabled.onChange.remove(hideIntroEnabledCheck);
  tabSize.onChange.remove(tabSizeCheck);
  remoteHost.onChange.remove(remoteHostCheck);

  function remoteHostReCheck(ev) {
    is(ev.setting, remoteHost, "remoteHost event reset");
    is(ev.value, null, "remoteHost event revalue");
    is(ev.setting.value, null, "remoteHost event setting revalue");
  }
  remoteHost.onChange.add(remoteHostReCheck);

  hideIntroEnabled.setDefault();
  tabSize.setDefault();
  remoteHost.setDefault();

  remoteHost.onChange.remove(remoteHostReCheck);

  is(hideIntroEnabled.value, hideIntroEnabledDefault, "hideIntroEnabled reset");
  is(tabSize.value, tabSizeDefault, "tabSize reset");
  is(remoteHost.value, remoteHostDefault, "remoteHost reset");

  // Cleanup
  prefBranch.setBoolPref("devtools.gcli.hideIntro", hideIntroOrig);
  prefBranch.setIntPref("devtools.editor.tabsize", tabSizeOrig);
  supportsString.data = remoteHostOrig;
  prefBranch.setComplexValue("devtools.debugger.remote-host",
          Components.interfaces.nsISupportsString,
          supportsString);

  yield helpers.closeTab(options);
}
