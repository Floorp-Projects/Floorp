/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* See https://bugzilla.mozilla.org/show_bug.cgi?id=780370 */

const Cu = Components.utils;

// Use a COW to expose a function from a standard prototype, and make we deny
// access to it.

function run_test()
{
  var sb = Cu.Sandbox("http://www.example.com");
  sb.obj = { foo: 42, __exposedProps__: { hasOwnProperty: 'r' } };
  do_check_eq(Cu.evalInSandbox('typeof obj.foo', sb), 'undefined', "COW works as expected");
  try {
    Cu.evalInSandbox('obj.hasOwnProperty', sb);
    do_check_true(false);
  } catch (e) {
    do_check_true(/privileged or cross-origin callable/i.test(e));
  }
}
