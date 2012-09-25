function run_test() {
  var cu = Components.utils;
  var sbMaster = cu.Sandbox(["http://www.a.com",
                           "http://www.b.com",
                           "http://www.d.com"]);
  var sbSubset = cu.Sandbox(["http://www.d.com",                           
                           "http://www.a.com"]);

  var sbA = cu.Sandbox("http://www.a.com");
  var sbB = cu.Sandbox("http://www.b.com");
  var sbC = cu.Sandbox("http://www.c.com");

  sbMaster.objA = cu.evalInSandbox("var obj = {prop1:200}; obj", sbA);
  sbMaster.objB = cu.evalInSandbox("var obj = {prop1:200}; obj", sbB);
  sbMaster.objC = cu.evalInSandbox("var obj = {prop1:200}; obj", sbC);
  sbMaster.objOwn = cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);
  
  sbMaster.objSubset = cu.evalInSandbox("var obj = {prop1:200}; obj", sbSubset);
  sbA.objMaster = cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);
  sbSubset.objMaster = cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);

  var ret;
  ret = cu.evalInSandbox("objA.prop1", sbMaster);
  do_check_eq(ret, 200);
  ret = cu.evalInSandbox("objB.prop1", sbMaster);
  do_check_eq(ret, 200);
  ret = cu.evalInSandbox("objSubset.prop1", sbMaster);
  do_check_eq(ret, 200);
  
  function evalAndCatch(str, sb) {
    try {
      ret = cu.evalInSandbox(str, sb);
      do_check_true(false, "unexpected pass")
    } catch (e) {    
      do_check_true(e.message && e.message.indexOf("Permission denied to access property") != -1);
    }  
  }
  
  evalAndCatch("objC.prop1", sbMaster);
  evalAndCatch("objMaster.prop1", sbA);
  evalAndCatch("objMaster.prop1", sbSubset);
  
  // Bug 777705:
  sbMaster.Components = cu.getComponentsForScope(sbMaster);
  Components.utils.evalInSandbox("Components.interfaces", sbMaster);
  do_check_true(true);
}
