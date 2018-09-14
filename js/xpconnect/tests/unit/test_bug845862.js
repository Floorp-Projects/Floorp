function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  Cu.evalInSandbox("this.foo = {}; Object.defineProperty(foo, 'bar', {get: function() {return {};}});", sb);
  var desc = Object.getOwnPropertyDescriptor(Cu.waiveXrays(sb.foo), 'bar');
  var b = desc.get();
  Assert.ok(b != XPCNativeWrapper(b), "results from accessor descriptors are waived");
}
