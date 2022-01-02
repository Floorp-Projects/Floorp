function run_test() {
  var sbMaster = Cu.Sandbox(["http://www.a.com",
                           "http://www.b.com",
                           "http://www.d.com"]);
  var sbSubset = Cu.Sandbox(["http://www.d.com",
                           "http://www.a.com"]);

  var sbA = Cu.Sandbox("http://www.a.com");
  var sbB = Cu.Sandbox("http://www.b.com");
  var sbC = Cu.Sandbox("http://www.c.com");

  sbMaster.objA = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbA);
  sbMaster.objB = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbB);
  sbMaster.objC = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbC);
  sbMaster.objOwn = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);
  
  sbMaster.objSubset = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbSubset);
  sbA.objMaster = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);
  sbSubset.objMaster = Cu.evalInSandbox("var obj = {prop1:200}; obj", sbMaster);

  var ret;
  ret = Cu.evalInSandbox("objA.prop1", sbMaster);
  Assert.equal(ret, 200);
  ret = Cu.evalInSandbox("objB.prop1", sbMaster);
  Assert.equal(ret, 200);
  ret = Cu.evalInSandbox("objSubset.prop1", sbMaster);
  Assert.equal(ret, 200);
  
  function evalAndCatch(str, sb) {
    try {
      ret = Cu.evalInSandbox(str, sb);
      Assert.ok(false, "unexpected pass")
    } catch (e) {    
      Assert.ok(e.message && e.message.includes("Permission denied to access property"));
    }  
  }
  
  evalAndCatch("objC.prop1", sbMaster);
  evalAndCatch("objMaster.prop1", sbA);
  evalAndCatch("objMaster.prop1", sbSubset);
}
