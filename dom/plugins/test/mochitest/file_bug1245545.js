/* eslint-env mozilla/frame-script */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function getTestPlugin(pluginName) {
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();
  var name = pluginName || "Test Plug-in";
  for (var tag of tags) {
    if (tag.name == name) {
      return tag;
    }
  }
  return null;
}

addMessageListener("check-plugin-unload", function(message) {
  var tag = getTestPlugin();
  sendAsyncMessage("check-plugin-unload", tag.loaded);
});
