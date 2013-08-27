/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

Object.defineProperty(this, 'global', { value: this });

exports.testGlobals = function(assert) {
  // the only globals in module scope should be:
  //   module, exports, require, dump, console
  assert.equal(typeof module, 'object', 'have "module", good');
  assert.equal(typeof exports, 'object', 'have "exports", good');
  assert.equal(typeof require, 'function', 'have "require", good');
  assert.equal(typeof dump, 'function', 'have "dump", good');
  assert.equal(typeof console, 'object', 'have "console", good');

  // in particular, these old globals should no longer be present
  assert.ok(!('packaging' in global), "no 'packaging', good");
  assert.ok(!('memory' in global), "no 'memory', good");
  assert.ok(/test-globals\.js$/.test(module.uri),
     'should contain filename');
};

exports.testComponent = function (assert) {
  assert.throws(() => {
    Components;
  }, /`Components` is not available/, 'using `Components` throws');
};

require('test').run(exports);
