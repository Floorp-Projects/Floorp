/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  do_get_profile_startup();

  var plugin = get_test_plugintag();
  do_check_true(plugin == null);

  // Initialises a profile folder
  do_get_profile();

  var plugin = get_test_plugintag();
  do_check_false(plugin == null);

  // Clean up
  Services.prefs.clearUserPref("plugin.importedState");
}
