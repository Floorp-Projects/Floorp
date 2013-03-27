/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function check_state(aTag, aExpectedClicktoplay, aExpectedDisabled) {
  do_check_eq(aTag.clicktoplay, aExpectedClicktoplay);
  do_check_eq(aTag.disabled, aExpectedDisabled);
}

function run_test() {
  let tag = get_test_plugintag();
  check_state(tag, false, false);

  /* test going to click-to-play from always enabled and back */
  tag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  check_state(tag, true, false);
  tag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  check_state(tag, false, false);

  /* test going to disabled from always enabled and back */
  tag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  check_state(tag, false, true);
  tag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  check_state(tag, false, false);

  /* test going to click-to-play from disabled and back */
  tag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  check_state(tag, false, true);
  tag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
  check_state(tag, true, false);
  tag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
  check_state(tag, false, true);

  /* put everything back to normal and check that that succeeded */
  tag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  check_state(tag, false, false);
}
