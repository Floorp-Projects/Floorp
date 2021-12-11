function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["CSS"] });
  sb.equal = equal;
  Cu.evalInSandbox('equal(CSS.escape("$"), "\\\\$");',
                   sb);
  Cu.importGlobalProperties(["CSS"]);
  Assert.equal(CSS.escape("$"), "\\$");
}
