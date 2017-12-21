/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["FileReader"] });
  sb.ok = ok;
  Cu.evalInSandbox('ok((new FileReader()) instanceof FileReader);',
                   sb);
  Cu.importGlobalProperties(["FileReader"]);
  Assert.ok((new FileReader()) instanceof FileReader);
}
