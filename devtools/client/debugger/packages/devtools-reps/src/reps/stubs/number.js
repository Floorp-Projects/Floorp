/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const stubs = new Map();
stubs.set("Int", 5);
stubs.set("True", true);
stubs.set("False", false);
stubs.set("NegZeroValue", -0);
stubs.set("NegZeroGrip", {
  type: "-0",
});
stubs.set("UnsafeInt", 900719925474099122);

module.exports = stubs;
