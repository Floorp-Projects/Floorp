/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var scope1 = {};
  var exports1 = ChromeUtils.import("resource://test/TestBlob.jsm", scope1);

  var scope2 = {};
  var exports2 = ChromeUtils.import("resource://test/TestBlob.jsm", scope2);

  Assert.ok(exports1 === exports2);
  Assert.ok(scope1.TestBlob === scope2.TestBlob);

  Cu.unload("resource://test/TestBlob.jsm");

  var scope3 = {};
  var exports3 = ChromeUtils.import("resource://test/TestBlob.jsm", scope3);

  Assert.equal(false, exports1 === exports3);
  Assert.equal(false, scope1.TestBlob === scope3.TestBlob);

  // When the jsm was unloaded, the value of all its global's properties were
  // set to undefined. While it must be safe (not crash) to call into the
  // module, we expect it to throw an error (e.g., when trying to use Ci).
  try { scope1.TestBlob.doTest(() => {}); } catch (e) {}
  try { scope3.TestBlob.doTest(() => {}); } catch (e) {}
}
