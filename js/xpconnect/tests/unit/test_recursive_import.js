/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {
  var scope = {};
  Components.utils.import("resource://test/recursive_importA.jsm", scope);

  // A imported correctly
  Assert.ok(scope.foo() == "foo");

  // Symbols from B are visible through A
  Assert.ok(scope.bar.baz() == "baz");

  // Symbols from A are visible through A, B, A.
  Assert.ok(scope.bar.qux.foo() == "foo");
}
