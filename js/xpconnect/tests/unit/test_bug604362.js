function run_test() {
  var sp = Cc["@mozilla.org/systemprincipal;1"].
           createInstance(Ci.nsIPrincipal);
  var s = Cu.Sandbox(sp);
  s.a = [];
  s.Cu = Cu;
  s.C = Components;
  s.notEqual = notEqual;
  Cu.evalInSandbox("notEqual(Cu.import, undefined);", s);
}
