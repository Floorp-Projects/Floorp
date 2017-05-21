/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=1354733 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const global = this;

function run_test()
{
  var sb = Cu.Sandbox(global);
  let obj = new sb.Object();
  Cu.nukeSandbox(sb);

  ok(Cu.isDeadWrapper(obj), "object should be a dead wrapper");

  // Create a new sandbox to wrap objects for.

  var sb = Cu.Sandbox(global);
  Cu.evalInSandbox(function echo(val) { return val; },
                   sb);

  let echoed = sb.echo(obj);
  ok(Cu.isDeadWrapper(echoed), "Rewrapped object should be a dead wrapper");
  ok(echoed !== obj, "Rewrapped object should be a new dead wrapper");

  ok(obj === obj, "Dead wrapper object should be equal to itself");

  let liveObj = {};
  ok(liveObj === sb.echo(liveObj), "Rewrapped live object should be equal to itself");
}
