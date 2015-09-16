/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var four = require("./modules/exportsEquals");
exports.testExportsEquals = function(assert) {
  assert.equal(four, 4);
};

/* TODO: Discuss idea of dropping support for this feature that was alternative
         to `module.exports = ..` that failed.
var five = require("./modules/setExports");
exports.testSetExports = function(assert) {
  assert.equal(five, 5);
}

exports.testDupeSetExports = function(assert) {
  var passed = false;
  try {
    var dupe = require('./modules/dupeSetExports');
  } catch(e) {
    passed = /define\(\) was used, so module\.exports= and module\.setExports\(\) may not be used/.test(e.toString());
  }
  assert.equal(passed, true, 'define() or setExports(), not both');
}
*/

exports.testModule = function(assert) {
  // module.id is not cast in stone yet. In the future, it may include the
  // package name, or may possibly be a/ URL of some sort. For now, it's a
  // URL that starts with resource: and ends with this module name, but the
  // part in between varies depending upon how the test is run.
  var found = /test-set-exports$/.test(module.id);
  assert.equal(found, true, module.id+" ends with test-set-exports.js");
};

require('sdk/test').run(exports);
