function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["CSS"] });
  sb.equal = equal;
  Cu.evalInSandbox('equal(CSS.escape("$"), "\\\\$");',
                   sb);
  Cu.importGlobalProperties(["CSS"]);
  Assert.equal(CSS.escape("$"), "\\$");
}
