function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantDOMConstructors: ["TextDecoder", "TextEncoder"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(new TextDecoder().encoding, "utf-8");' +
                   'do_check_eq(new TextEncoder().encoding, "utf-8");',
                   sb);
}
