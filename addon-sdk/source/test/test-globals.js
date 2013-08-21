/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

Object.defineProperty(this, 'global', { value: this });

exports.testGlobals = function(assert) {
  // the only globals in module scope should be:
  assert.equal(typeof module, 'object', 'have "module" global');
  assert.equal(typeof exports, 'object', 'have "exports" global');
  assert.equal(typeof require, 'function', 'have "require" global');
  assert.equal(typeof dump, 'function', 'have "dump" global');
  assert.equal(typeof console, 'object', 'have "console" global');

  // in particular, these old globals should no longer be present
  assert.ok(!('packaging' in global), 'no "packaging" global was found');
  assert.ok(!('memory' in global), 'no "memory" global was found');

  assert.ok(/test-globals\.js$/.test(module.uri), 'should contain filename');
};

require("test").run(exports);
