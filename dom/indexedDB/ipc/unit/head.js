/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const INDEXEDDB_UNIT_DIR = "../../test/unit/";
const INDEXEDDB_HEAD_FILE = INDEXEDDB_UNIT_DIR + "head.js";

function run_test() {
  // IndexedDB needs a profile.
  do_get_profile();

  let thisTest = _TEST_FILE.toString().replace(/\\/g, "/");
  thisTest = thisTest.substring(thisTest.lastIndexOf("/") + 1);

  _HEAD_FILES.push(do_get_file(INDEXEDDB_HEAD_FILE).path.replace(/\\/g, "/"));
  run_test_in_child(INDEXEDDB_UNIT_DIR + thisTest);
}
