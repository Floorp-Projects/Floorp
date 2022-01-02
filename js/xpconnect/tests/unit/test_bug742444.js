function run_test() {
  let sb1A = Cu.Sandbox('http://www.example.com');
  let sb1B = Cu.Sandbox('http://www.example.com');
  let sb2 = Cu.Sandbox('http://www.example.org');
  let sbChrome = Cu.Sandbox(this);
  let obj = new sb1A.Object();
  sb1B.obj = obj;
  sb1B.waived = Cu.waiveXrays(obj);
  sb2.obj = obj;
  sb2.waived = Cu.waiveXrays(obj);
  sbChrome.obj = obj;
  sbChrome.waived = Cu.waiveXrays(obj);
  Assert.ok(Cu.evalInSandbox('obj === waived', sb1B));
  Assert.ok(Cu.evalInSandbox('obj === waived', sb2));
  Assert.ok(Cu.evalInSandbox('obj !== waived', sbChrome));
}
