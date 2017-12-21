function run_test() {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var sp = Cc["@mozilla.org/systemprincipal;1"].
           createInstance(Ci.nsIPrincipal);
  var s = Components.utils.Sandbox(sp);
  s.a = [];
  s.Cu = Components.utils;
  s.C = Components;
  s.notEqual = notEqual;
  Components.utils.evalInSandbox("notEqual(Cu.import, undefined);", s);
}
