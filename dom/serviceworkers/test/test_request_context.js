// Copied from /dom/plugins/test/mochitest/utils.js
function getTestPlugin(pluginName) {
  var ph = SpecialPowers.Cc["@mozilla.org/plugin/host;1"]
                                 .getService(SpecialPowers.Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = pluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }

  ok(false, "Could not find plugin tag with plugin name '" + name + "'");
  return null;
}
function setTestPluginEnabledState(newEnabledState, pluginName) {
  var oldEnabledState = SpecialPowers.setTestPluginEnabledState(newEnabledState, pluginName);
  if (!oldEnabledState) {
    return;
  }
  var plugin = getTestPlugin(pluginName);
  // Run a nested event loop to wait for the preference change to
  // propagate to the child. Yuck!
  SpecialPowers.Services.tm.spinEventLoopUntil(() => plugin.enabledState == newEnabledState);
  SimpleTest.registerCleanupFunction(function() {
    SpecialPowers.setTestPluginEnabledState(oldEnabledState, pluginName);
  });
}
setTestPluginEnabledState(SpecialPowers.Ci.nsIPluginTag.STATE_ENABLED);

function isMulet() {
  try {
    return SpecialPowers.getBoolPref("b2g.is_mulet");
  } catch(e) {
    return false;
  }
}

var iframe;
function runTest() {
  iframe = document.querySelector("iframe");
  iframe.src = "/tests/dom/workers/test/serviceworkers/fetch/context/register.html";
  window.onmessage = function(e) {
    if (e.data.status == "ok") {
      ok(e.data.result, e.data.message);
    } else if (e.data.status == "todo") {
      todo(e.data.result, e.data.message);
    } else if (e.data.status == "registrationdone") {
      iframe.src = "/tests/dom/workers/test/serviceworkers/fetch/context/index.html?" + gTest;
    } else if (e.data.status == "done") {
      iframe.src = "/tests/dom/workers/test/serviceworkers/fetch/context/unregister.html";
    } else if (e.data.status == "unregistrationdone") {
      window.onmessage = null;
      ok(true, "Test finished successfully");
      SimpleTest.finish();
    }
  };
}

SimpleTest.waitForExplicitFinish();
onload = function() {
  SpecialPowers.pushPrefEnv({"set": [
    ["beacon.enabled", true],
    ["browser.send_pings", true],
    ["browser.send_pings.max_per_link", -1],
    ["dom.caches.enabled", true],
    ["dom.requestcontext.enabled", true],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]}, runTest);
};
