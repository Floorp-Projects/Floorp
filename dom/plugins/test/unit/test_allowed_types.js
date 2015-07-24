/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  const pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  const pluginDefaultState = Services.prefs.getIntPref("plugin.default.state");

  function reload_plugins_with_allowed_types(allowed_types) {
    if (typeof allowed_types === "undefined") {
      // If we didn't get an allowed_types string, then unset the pref,
      // so the caller can test the behavior when the pref isn't set.
      Services.prefs.clearUserPref("plugin.allowed_types");
    } else {
      Services.prefs.setCharPref("plugin.allowed_types", allowed_types);
    }
    pluginHost.reloadPlugins();
  }

  function get_status_for_type(type) {
    try {
      return pluginHost.getStateForType(type);
    } catch(ex) {
      // If the type is not allowed, then nsIPluginHost.getStateForType throws
      // NS_ERROR_NOT_AVAILABLE, for which we return undefined to make it easier
      // to write assertions about the API.
      if (ex.result === Cr.NS_ERROR_NOT_AVAILABLE) {
        return undefined;
      }
      throw ex;
    }
  }

  // If allowed_types isn't set, then all plugin types are enabled.
  reload_plugins_with_allowed_types();
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  // If allowed_types is set to the empty string, then all plugin types are enabled.
  reload_plugins_with_allowed_types("");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  // If allowed_types is set to anything other than the empty string,
  // then only types that exactly match its comma-separated entries are enabled,
  // so a single space disables all types.
  reload_plugins_with_allowed_types(" ");
  do_check_eq(get_status_for_type("application/x-test"), undefined);
  do_check_eq(get_status_for_type("application/x-Second-Test"), undefined);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  // The rest of the assertions test various values of allowed_types to ensure
  // that the correct types are enabled.

  reload_plugins_with_allowed_types("application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), undefined);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-Second-Test");
  do_check_eq(get_status_for_type("application/x-test"), undefined);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-nonexistent");
  do_check_eq(get_status_for_type("application/x-test"), undefined);
  do_check_eq(get_status_for_type("application/x-Second-Test"), undefined);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-test,application/x-Second-Test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-Second-Test,application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-test,application/x-nonexistent");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), undefined);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-nonexistent,application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), undefined);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-test,application/x-Second-Test,application/x-nonexistent");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-test,application/x-nonexistent,application/x-Second-Test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-Second-Test,application/x-test,application/x-nonexistent");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-Second-Test,application/x-nonexistent,application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-nonexistent,application/x-test,application/x-Second-Test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  reload_plugins_with_allowed_types("application/x-nonexistent,application/x-Second-Test,application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-Second-Test"), pluginDefaultState);
  do_check_eq(get_status_for_type("application/x-nonexistent"), undefined);

  // Plugin types are case-insensitive, so matching should be too.
  reload_plugins_with_allowed_types("APPLICATION/X-TEST");
  do_check_eq(get_status_for_type("application/x-test"), pluginDefaultState);
  reload_plugins_with_allowed_types("application/x-test");
  do_check_eq(get_status_for_type("APPLICATION/X-TEST"), pluginDefaultState);

  // Types must match exactly, so supersets should not enable subset types.
  reload_plugins_with_allowed_types("application/x-test-superset");
  do_check_eq(get_status_for_type("application/x-test"), undefined);
  reload_plugins_with_allowed_types("superset-application/x-test");
  do_check_eq(get_status_for_type("application/x-test"), undefined);

  // Clean up.
  Services.prefs.clearUserPref("plugin.allowed_types");
  Services.prefs.clearUserPref("plugin.importedState");
}
