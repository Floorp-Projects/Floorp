function run_test() {
  Assert.ok(Cu.getObjectPrincipal({}).isSystemPrincipal);
  var sb = new Cu.Sandbox('http://www.example.com');
  Cu.evalInSandbox('var obj = { foo: 42 };', sb);
  Assert.equal(Cu.getObjectPrincipal(sb.obj).origin, 'http://www.example.com');
}
