function run_test() {
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["atob", "btoa"] });
  sb.equal = equal;
  Cu.evalInSandbox('var dummy = "Dummy test.";' +
                   'equal(dummy, atob(btoa(dummy)));' +
                   'equal(btoa("budapest"), "YnVkYXBlc3Q=");',
                   sb);
}
