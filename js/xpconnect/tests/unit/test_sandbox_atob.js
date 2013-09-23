function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["atob", "btoa"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('var dummy = "Dummy test.";' +
                   'do_check_eq(dummy, atob(btoa(dummy)));' +
                   'do_check_eq(btoa("budapest"), "YnVkYXBlc3Q=");',
                   sb);
}
