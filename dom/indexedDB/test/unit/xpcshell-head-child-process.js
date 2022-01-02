/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  const INDEXEDDB_HEAD_FILE = "xpcshell-head-parent-process.js";
  const INDEXEDDB_PREF_EXPERIMENTAL = "dom.indexedDB.experimental";

  // IndexedDB needs a profile.
  do_get_profile();

  let thisTest = _TEST_FILE.toString().replace(/\\/g, "/");
  thisTest = thisTest.substring(thisTest.lastIndexOf("/") + 1);

  // This is defined globally via xpcshell.
  /* global _HEAD_FILES */
  _HEAD_FILES.push(do_get_file(INDEXEDDB_HEAD_FILE).path.replace(/\\/g, "/"));

  Services.prefs.setBoolPref(INDEXEDDB_PREF_EXPERIMENTAL, true);

  run_test_in_child(thisTest);
}
