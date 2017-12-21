function run_test() {
  var Cu = Components.utils;
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["TextDecoder", "TextEncoder"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(new TextDecoder().encoding, "utf-8");' +
                   'do_check_eq(new TextEncoder().encoding, "utf-8");',
                   sb);
  Cu.importGlobalProperties(["TextDecoder", "TextEncoder"]);
  Assert.equal(new TextDecoder().encoding, "utf-8");
  Assert.equal(new TextEncoder().encoding, "utf-8");
}
