function run_test() {
  sb = new Cu.Sandbox('http://www.example.com');
  sb.equal = equal;
  Cu.evalInSandbox('equal(typeof new Promise(function(resolve){resolve();}), "object");',
                   sb);
  Assert.equal(typeof new Promise(function(resolve){resolve();}), "object");
}
