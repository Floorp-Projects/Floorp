function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["CSS"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(CSS.escape("$"), "\\\\$");',
                   sb);
  Cu.importGlobalProperties(["CSS"]);
  do_check_eq(CSS.escape("$"), "\\$");
}
