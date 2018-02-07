function run_test() {
  sb = new Cu.Sandbox('http://www.example.com',
                      { wantGlobalProperties: ["TextDecoder", "TextEncoder"] });
  sb.equal = equal;
  Cu.evalInSandbox('equal(new TextDecoder().encoding, "utf-8");' +
                   'equal(new TextEncoder().encoding, "utf-8");',
                   sb);
  Cu.importGlobalProperties(["TextDecoder", "TextEncoder"]);
  Assert.equal(new TextDecoder().encoding, "utf-8");
  Assert.equal(new TextEncoder().encoding, "utf-8");
}
