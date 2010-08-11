function run_test() {
  return;
  var jetpack = createJetpack({
    scriptFile: do_get_file("impl_rooting.js")
  });
  jetpack.registerReceiver("onInvalidateReceived", function(name, isValid) {
    do_check_false(isValid, "onInvalidateReceived: isValid");
    do_test_finished();
  });
  var gchandle = jetpack.createHandle();
  jetpack.sendMessage("ReceiveGCHandle", gchandle);
  gchandle.isRooted = false;
  gchandle = null;
  do_execute_soon(gc);
  do_test_pending();
}
