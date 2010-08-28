function run_test() {
  var jetpack = createJetpack({
    scriptFile: do_get_file("impl_jetpack_ctypes.js")
  });
  jetpack.registerReceiver("onCTypesTested", function(name, ok) {
    do_check_true(ok, "onCTypesTested");
    do_test_finished();
  });
  var jetpacktestdir = do_get_file('../../../../toolkit/components/ctypes/tests/unit').path;
  jetpack.sendMessage("testCTypes", jetpacktestdir);
  do_test_pending();
}
