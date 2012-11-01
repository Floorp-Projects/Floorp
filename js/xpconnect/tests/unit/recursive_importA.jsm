/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["foo", "bar"];

function foo() {
  return "foo";
}

var bar = {}
Components.utils.import("resource://test/recursive_importB.jsm", bar);
