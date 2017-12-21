const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  var obj = Cu.evalInSandbox('this.obj = {foo: 2}; obj', sb);
  var chromeSb = new Cu.Sandbox(this);
  chromeSb.objRef = obj;
  Assert.equal(Cu.evalInSandbox('objRef.foo', chromeSb), 2);
  Cu.nukeSandbox(sb);
  Assert.ok(Cu.isDeadWrapper(obj));
  // CCWs to nuked wrappers should be considered dead.
  Assert.ok(Cu.isDeadWrapper(chromeSb.objRef));
}
