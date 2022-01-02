/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  Assert.throws(
    () => Cu.printStderr(),
    /Not enough arguments/,
    "Without argument printStderr throws"
  );

  const types = ["", "foo", 42, true, null, {foo: "bar"}];
  types.forEach(type => Cu.printStderr(type));
});
