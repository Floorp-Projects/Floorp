/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["FileReader"] });
  sb.do_check_true = do_check_true;
  Cu.evalInSandbox('do_check_true((new FileReader()) instanceof FileReader);',
                   sb);
  Cu.importGlobalProperties(["FileReader"]);
  do_check_true((new FileReader()) instanceof FileReader);
}
