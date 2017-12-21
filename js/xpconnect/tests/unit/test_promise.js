function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com');
  sb.equal = equal;
  Cu.evalInSandbox('equal(typeof new Promise(function(resolve){resolve();}), "object");',
                   sb);
  Assert.equal(typeof new Promise(function(resolve){resolve();}), "object");
}
