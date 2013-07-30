/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { setTimeout } = require('sdk/timers');

let mainStarted = false;

exports.main = function main(options, callbacks) {
  mainStarted = true;

  let tests = {};

  tests.testMainArguments = function(assert) {
  	assert.ok(!!options, 'options argument provided to main');
    assert.ok('loadReason' in options, 'loadReason is in options provided by main');
    assert.equal(typeof callbacks.print, 'function', 'callbacks.print is a function');
    assert.equal(typeof callbacks.quit, 'function', 'callbacks.quit is a function');
    assert.equal(options.loadReason, 'install', 'options.loadReason is install');
  }

  require('sdk/test/runner').runTestsFromModule({exports: tests});
}

// this causes a fail if main does not start
setTimeout(function() {
  if (mainStarted)
  	return;

  // main didn't start, fail..
  require("sdk/test/runner").runTestsFromModule({exports: {
  	testFail: function(assert) assert.fail('Main did not start..')
  }});
}, 500);
