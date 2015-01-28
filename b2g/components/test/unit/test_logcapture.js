/**
 * Testing non Gonk-specific code path
 */
function run_test() {
  Components.utils.import("resource:///modules/LogCapture.jsm");
  run_next_test();
}

// Trivial test just to make sure we have no syntax error
add_test(function test_logCapture_loads() {
  ok(LogCapture, "LogCapture object exists");
  run_next_test();
});
