/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["baz", "qux"];

function baz() {
  return "baz";
}

var qux = {}
Components.utils.import("resource://test/recursive_importA.jsm", qux);

