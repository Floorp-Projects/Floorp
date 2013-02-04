/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Object.defineProperty(this, "global", { value: this });

exports.testGlobals = function(test) {
  // the only globals in module scope should be:
  //   module, exports, require, dump, console
  test.assertObject(module, "have 'module', good");
  test.assertObject(exports, "have 'exports', good");
  test.assertFunction(require, "have 'require', good");
  test.assertFunction(dump, "have 'dump', good");
  test.assertObject(console, "have 'console', good");

  // in particular, these old globals should no longer be present
  test.assert(!('packaging' in global), "no 'packaging', good");
  test.assert(!('memory' in global), "no 'memory', good");

  test.assertMatches(module.uri, /test-globals\.js$/,
                     'should contain filename');
};
