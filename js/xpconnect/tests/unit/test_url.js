function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["URL"] });
  sb.equal = equal;
  Cu.evalInSandbox('equal(new URL("http://www.example.com").host, "www.example.com");',
                   sb);
  Cu.importGlobalProperties(["URL"]);
  Assert.equal(new URL("http://www.example.com").host, "www.example.com");
}
