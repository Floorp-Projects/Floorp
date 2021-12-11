/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=1298597 */

function run_test()
{
  var sb = Cu.Sandbox("http://www.blah.com");
  var resolveFun;
  var p1 = new sb.Promise((res, rej) => {resolveFun = res});
  var rejectFun;
  var p2 = new sb.Promise((res, rej) => {rejectFun = rej});
  Cu.nukeSandbox(sb);
  Assert.ok(Cu.isDeadWrapper(sb), "sb should be dead");
  Assert.ok(Cu.isDeadWrapper(p1), "p1 should be dead");
  Assert.ok(Cu.isDeadWrapper(p2), "p2 should be dead");

  var exception;

  try{
    resolveFun(1);
    Assert.ok(false);
  } catch (e) {
    exception = e;
  }
  Assert.ok(exception.toString().includes("can't access dead object"),
            "Resolving dead wrapped promise should throw");

  exception = undefined;
  try{
    rejectFun(1);
    Assert.ok(false);
  } catch (e) {
    exception = e;
  }
  Assert.ok(exception.toString().includes("can't access dead object"),
            "Rejecting dead wrapped promise should throw");
}
