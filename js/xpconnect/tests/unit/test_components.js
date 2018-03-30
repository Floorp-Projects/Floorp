function run_test() {
  var sb1 = Cu.Sandbox("http://www.blah.com");
  var sb2 = Cu.Sandbox(this);
  var rv;

  // non-chrome accessing chrome Components
  sb1.C = Components;
  checkThrows("C.interfaces", sb1);
  checkThrows("C.utils", sb1);
  checkThrows("C.classes", sb1);

  // non-chrome accessing own Components: shouldn't exist.
  Assert.equal(Cu.evalInSandbox("typeof Components", sb1), 'undefined');

  // chrome accessing chrome
  sb2.C = Components;
  rv = Cu.evalInSandbox("C.utils", sb2);
  Assert.equal(rv, Cu);
}

function checkThrows(expression, sb) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + '; return "allowed"; } catch (e) { return e.toString(); }})();', sb);
  Assert.ok(!!/denied/.exec(result));
}
