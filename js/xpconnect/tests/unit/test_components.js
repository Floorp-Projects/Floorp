const Cu = Components.utils;

function run_test() {
  var sb1 = Cu.Sandbox("http://www.blah.com");
  var sb2 = Cu.Sandbox("http://www.blah.com");
  var sb3 = Cu.Sandbox(this);
  var sb4 = Cu.Sandbox("http://www.other.com");
  var rv;

  // Components is normally hidden from content on the XBL scope chain, but we
  // expose it to content here to make sure that the security wrappers work
  // regardless.
  [sb1, sb2, sb4].forEach(function(x) { x.Components = Cu.getComponentsForScope(x); });

  // non-chrome accessing chrome Components
  sb1.C = Components;
  checkThrows("C.utils", sb1);
  checkThrows("C.classes", sb1);

  // non-chrome accessing own Components
  checkThrows("Components.utils", sb1);
  checkThrows("Components.classes", sb1);

  // non-chrome same origin
  var C2 = Cu.evalInSandbox("Components", sb2);
  do_check_neq(rv, C2.utils);
  sb1.C2 = C2;
  checkThrows("C2.utils", sb1);
  checkThrows("C2.classes", sb1);

  // chrome accessing chrome
  sb3.C = Components;
  rv = Cu.evalInSandbox("C.utils", sb3);
  do_check_eq(rv, Cu);

  // non-chrome cross origin
  sb4.C2 = C2;
  checkThrows("C2.utils", sb1);
  checkThrows("C2.classes", sb1);
}

function checkThrows(expression, sb) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + '; return "allowed"; } catch (e) { return e.toString(); }})();', sb);
  do_check_true(!!/denied/.exec(result));
}
