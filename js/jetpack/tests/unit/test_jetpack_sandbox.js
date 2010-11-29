function run_test() {
    var jetpack = createJetpack({ scriptFile: do_get_file("impl.js") });
  
  var sandbox = Components.utils.Sandbox("about:blank");
  function registerReceiver(name, fn) {
    jetpack.registerReceiver(name, fn);
  }
  sandbox.registerReceiver = registerReceiver;
  sandbox.echoed = function(message, arg) {
    do_check_eq(message, "echo");
    do_check_eq(arg, "testdata");
    jetpack.destroy();
    sandbox = null;
    do_test_finished();
  };
  Components.utils.evalInSandbox("registerReceiver('echo', function(message, arg){ echoed(message, arg); });", sandbox);
  jetpack.sendMessage("echo", "testdata");
  do_test_pending();
}
