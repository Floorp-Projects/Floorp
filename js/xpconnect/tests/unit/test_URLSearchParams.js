/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["URLSearchParams"] });
  sb.do_check_eq = do_check_eq;
  Cu.evalInSandbox('do_check_eq(new URLSearchParams("one=1&two=2").get("one"), "1");',
                   sb);
  Cu.importGlobalProperties(["URLSearchParams"]);
  do_check_eq(new URLSearchParams("one=1&two=2").get("one"), "1");
}
