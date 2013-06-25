const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  var obj = Cu.evalInSandbox('this.obj = {foo: 2}; obj', sb);
  var chromeSb = new Cu.Sandbox(this);
  chromeSb.objRef = obj;
  do_check_eq(Cu.evalInSandbox('objRef.foo', chromeSb), 2);
  Cu.nukeSandbox(sb);
  do_check_true(Cu.isDeadWrapper(obj));
  // CCWs to nuked wrappers should be considered dead.
  do_check_true(Cu.isDeadWrapper(chromeSb.objRef));
}
