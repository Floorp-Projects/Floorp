/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=780370 */

const Cu = Components.utils;

// Use a COW to expose a function from a standard prototype, and make sure it's
// still remapped.

function run_test()
{
  var sb = Cu.Sandbox("http://www.example.com");
  sb.obj = { foo: 42, __exposedProps__: { hasOwnProperty: 'r' } };
  do_check_eq(Cu.evalInSandbox('typeof obj.foo', sb), 'undefined', "COW works as expected");
  do_check_true(Cu.evalInSandbox('obj.hasOwnProperty === Object.prototype.hasOwnProperty', sb),
                "Remapping happens even when the property is explicitly exposed");
  do_check_eq(Cu.evalInSandbox('Object.prototype.bar = 10; obj.bar', sb), 10);
}
