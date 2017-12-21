const Cu = Components.utils;
function run_test() {
  var sb = new Cu.Sandbox(null);
  Assert.ok(Cu.getObjectPrincipal(sb).isNullPrincipal);
}
