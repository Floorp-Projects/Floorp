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
  Assert.equal(Cu.evalInSandbox("typeof Components.interfaces", sb1), 'object');
  Assert.equal(Cu.evalInSandbox("typeof Components.utils", sb1), 'undefined');
  Assert.equal(Cu.evalInSandbox("typeof Components.classes", sb1), 'undefined');

  // Make sure an unprivileged Components is benign.
  var C2 = Cu.evalInSandbox("Components", sb2);
  var whitelist = ['interfaces', 'interfacesByID', 'results', 'isSuccessCode', 'QueryInterface'];
  for (var prop in Components) {
    do_print("Checking " + prop);
    Assert.equal((prop in C2), whitelist.indexOf(prop) != -1);
  }

  // non-chrome same origin
  sb1.C2 = C2;
  Assert.equal(Cu.evalInSandbox("typeof C2.interfaces", sb1), 'object');
  Assert.equal(Cu.evalInSandbox("typeof C2.utils", sb1), 'undefined');
  Assert.equal(Cu.evalInSandbox("typeof C2.classes", sb1), 'undefined');

  // chrome accessing chrome
  sb3.C = Components;
  rv = Cu.evalInSandbox("C.utils", sb3);
  Assert.equal(rv, Cu);

  // non-chrome cross origin
  sb4.C2 = C2;
  checkThrows("C2.interfaces", sb4);
  checkThrows("C2.utils", sb4);
  checkThrows("C2.classes", sb4);
}

function checkThrows(expression, sb) {
  var result = Cu.evalInSandbox('(function() { try { ' + expression + '; return "allowed"; } catch (e) { return e.toString(); }})();', sb);
  Assert.ok(!!/denied/.exec(result));
}
