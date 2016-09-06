/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=1298597 */

function run_test()
{
  var sb = Components.utils.Sandbox("http://www.blah.com");
  var resolveFun;
  var p1 = new sb.Promise((res, rej) => {resolveFun = res});
  var rejectFun;
  var p2 = new sb.Promise((res, rej) => {rejectFun = rej});
  Components.utils.nukeSandbox(sb);
  do_check_true(Components.utils.isDeadWrapper(sb), "sb should be dead");
  do_check_true(Components.utils.isDeadWrapper(p1), "p1 should be dead");
  do_check_true(Components.utils.isDeadWrapper(p2), "p2 should be dead");

  var exception;

  try{
    resolveFun(1);
    do_check_true(false);
  } catch (e) {
    exception = e;
  }
  do_check_true(exception.toString().includes("can't access dead object"),
                "Resolving dead wrapped promise should throw");

  exception = undefined;
  try{
    rejectFun(1);
    do_check_true(false);
  } catch (e) {
    exception = e;
  }
  do_check_true(exception.toString().includes("can't access dead object"),
                "Rejecting dead wrapped promise should throw");
}
