const Cu = Components.utils;

function run_test() {
  // We rely on the crazy "wantXrays:false also causes values return from the
  // sandbox to be waived" behavior, because it's the simplest way to get
  // waivers out of the sandbox (which has no native objects). :-(
  var sb = new Cu.Sandbox('http://www.example.com', {wantXrays: false});
  Cu.evalInSandbox("this.foo = {}; Object.defineProperty(foo, 'bar', {get: function() {return {};}});", sb);
  Assert.ok(sb.foo != XPCNativeWrapper(sb.foo), "sb.foo is waived");
  var desc = Object.getOwnPropertyDescriptor(sb.foo, 'bar');
  var b = desc.get();
  Assert.ok(b != XPCNativeWrapper(b), "results from accessor descriptors are waived");
}
