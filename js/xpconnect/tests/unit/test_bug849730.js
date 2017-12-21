const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com');
  sb.arr = [3, 4];
  Assert.ok(Cu.evalInSandbox('!Array.isArray(arr);', sb));
}
