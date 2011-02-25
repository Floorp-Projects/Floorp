g = { 'global noneval': 1 };
netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

eval("g['global eval'] = 1");

// Function to step through and set breakpoints on
function f1() {
  g['function noneval'] = 1;
  eval("g['function eval'] = 1");

  x = 1;
  for (y = 0; y < 10; y++) {
    x++;
  }
  for (y = 0; y < 3; y++) {
    x++;
  }
  z = 3;
}

var f2 = new Function("g['function noneval'] = 2; eval(\"g['function eval'] = 2\")");

function testJSD(jsd) {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    ok(jsd.isOn, "JSD needs to be running for this test.");

    var numBreakpoints = 0;

    f1();
    f2();
    jsd.topLevelHook = null;
    jsd.functionHook = null;
    dump("numGlobalNonevals="+numGlobalNonevals+"\n");
    dump("numFunctionNonevals="+numFunctionNonevals+"\n");
    dump("numGlobalEvals="+numGlobalEvals+"\n");
    dump("numFunctionEvals="+numFunctionEvals+"\n");

    ok(numFunctionNonevals == 3, "(fn) Should have hit f1(), testJSD(), and f2(); hit " + hits.fn);
    ok(numGlobalNonevals == 1, "(gn) Overall script, hit " + hits.gn);
    ok(numGlobalEvals == 1, "(ge) Eval in global area, hit " + hits.ge);
    ok(numFunctionEvals == 2, "(fe) Evals within f1() and f2(), hit " + hits.fe);

    if (!jsdOnAtStart) {
        // turn JSD off if it wasn't on when this test started
        jsd.off();
        ok(!jsd.isOn, "JSD shouldn't be running at the end of this test.");
    }

    SimpleTest.finish();
}

testJSD(jsd);
