//
// Run test script in content process instead of chrome (xpcshell's default)
//
function run_test() {
  /* globals do_run_test */
  load("../unit/test_resolve_uris.js");
  do_run_test();
  run_test_in_child("../unit/test_resolve_uris.js");
}
