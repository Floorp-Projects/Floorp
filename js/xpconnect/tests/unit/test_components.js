function run_test() {
  var Cu = Components.utils;
  var sb1 = Cu.Sandbox("http://www.blah.com");
  var sb2 = Cu.Sandbox("http://www.blah.com");
  var sb3 = Cu.Sandbox(this);
  var sb4 = Cu.Sandbox("http://www.other.com");
  var rv;

  // non-chrome accessing chrome Components
  sb1.C = Components;
  rv = Cu.evalInSandbox("C.utils", sb1);
  do_check_eq(rv, undefined);  
  rv = Cu.evalInSandbox("C.interfaces", sb1);
  do_check_neq(rv, undefined);

  // non-chrome accessing own Components
  rv = Cu.evalInSandbox("Components.utils", sb1);
  do_check_eq(rv, undefined);
  rv = Cu.evalInSandbox("Components.interfaces", sb1);
  do_check_neq(rv, undefined); 

  // non-chrome same origin
  var C2 = Cu.evalInSandbox("Components", sb2);
  do_check_neq(rv, C2.utils); 
  sb1.C2 = C2;
  rv = Cu.evalInSandbox("C2.utils", sb1);
  do_check_eq(rv, undefined);
  rv = Cu.evalInSandbox("C2.interfaces", sb1);
  do_check_neq(rv, undefined);

  // chrome accessing chrome
  sb3.C = Components;
  rv = Cu.evalInSandbox("C.utils", sb3);
  do_check_eq(rv, Cu);

  // non-chrome cross origin
  sb4.C2 = C2;
  rv = Cu.evalInSandbox("C2.interfaces", sb1);
  do_check_neq(rv, undefined);
  rv = Cu.evalInSandbox("C2.utils", sb1);
  do_check_eq(rv, undefined);

}