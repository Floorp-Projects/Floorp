function run_test() {
  var sb1 = Cu.Sandbox('http://www.example.com', { wantXrays: true });
  var sb2 = Cu.Sandbox('http://www.example.com', { wantXrays: false });
  sb2.f = Cu.evalInSandbox('x => typeof x', sb1);
  Assert.equal(Cu.evalInSandbox('f(dump)', sb2), 'function');
  Assert.equal(Cu.evalInSandbox('f.call(null, dump)', sb2), 'function');
  Assert.equal(Cu.evalInSandbox('f.apply(null, [dump])', sb2), 'function');
}
