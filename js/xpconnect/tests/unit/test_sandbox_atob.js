function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["atob", "btoa"] });
  sb.equal = equal;
  Cu.evalInSandbox('var dummy = "Dummy test.";' +
                   'equal(dummy, atob(btoa(dummy)));' +
                   'equal(btoa("budapest"), "YnVkYXBlc3Q=");',
                   sb);
}
