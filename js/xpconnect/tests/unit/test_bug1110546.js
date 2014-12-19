const Cu = Components.utils;
function run_test() {
  var sb = new Cu.Sandbox(null);
  do_check_true(Cu.getObjectPrincipal(sb).isNullPrincipal);
}
