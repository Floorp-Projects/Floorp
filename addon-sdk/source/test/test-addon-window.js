/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

var { Loader } = require('sdk/test/loader');

exports.testReady = function(assert, done) {
  let loader = Loader(module);
  let { ready, window } = loader.require('sdk/addon/window');
  let windowIsReady = false;

  ready.then(function() {
    assert.equal(windowIsReady, false, 'ready promise was resolved only once');
    windowIsReady = true;

    loader.unload();
    done();
  }).then(null, assert.fail);
}

require('sdk/test').run(exports);
