/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Finds the test plugin library
function get_test_plugin() {
  var plugins = gDirSvc.get("CurProcD", Ci.nsILocalFile);
  plugins.append("plugins");
  do_check_true(plugins.exists());
  var plugin = plugins.clone();
  // OSX plugin
  plugin.append("Test.plugin");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  plugin = plugins.clone();
  // *nix plugin
  plugin.append("libnptest.so");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  // Windows plugin
  plugin = plugins.clone();
  plugin.append("nptest.dll");
  if (plugin.exists()) {
    plugin.normalize();
    return plugin;
  }
  return null;
}
