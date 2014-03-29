function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com');
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(typeof new Promise(function(resolve){resolve();}), "object");',
                   sb);
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["-Promise"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(typeof Promise, "undefined");', sb);
  do_check_eq(typeof new Promise(function(resolve){resolve();}), "object");
}
