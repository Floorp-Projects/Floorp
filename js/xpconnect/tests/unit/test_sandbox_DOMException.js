/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  var Cu = Components.utils;
  var sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["DOMException"] });
  sb.notEqual = Assert.notEqual.bind(Assert);
  Cu.evalInSandbox('notEqual(DOMException, undefined);', sb);
}
