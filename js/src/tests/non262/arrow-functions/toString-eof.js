/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test(code) {
  assertEq(eval(code).toString(), code);

  assertEq(eval(code + ` `).toString(), code);
  assertEq(eval(code + ` \n`).toString(), code);

  assertEq(eval(code + ` // foo`).toString(), code);
  assertEq(eval(code + ` // foo
`).toString(), code);
  assertEq(eval(code + ` // foo
// foo`).toString(), code);
  assertEq(eval(code + ` // foo
// foo
`).toString(), code);
}
test(`() => 1`);
test(`() => () => 2`);
test(`() => class {}`);
test(`() => function() {}`);

if (typeof reportCompare === "function")
  reportCompare(true, true);
