const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  let w = Cu.evalInSandbox('var w = new WeakMap(); w.__proto__ = new Set(); w.foopy = 12; w', sb);
  Assert.equal(Object.getPrototypeOf(w), sb.Object.prototype);
  Assert.equal(Object.getOwnPropertyNames(w).length, 0);
  Assert.equal(w.wrappedJSObject.foopy, 12);
  Assert.equal(w.foopy, undefined);
}
