const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  let w = Cu.evalInSandbox('var w = WeakMap(); w.__proto__ = new Set(); w.foopy = 12; w', sb);
  do_check_eq(Object.getPrototypeOf(w), sb.Object.prototype);
  do_check_eq(Object.getOwnPropertyNames(w).length, 0);
  do_check_eq(w.wrappedJSObject.foopy, 12);
  do_check_eq(w.foopy, undefined);
}
