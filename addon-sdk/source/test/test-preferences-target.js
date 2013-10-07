/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { PrefsTarget } = require('sdk/preferences/event-target');
const { get, set, reset } = require('sdk/preferences/service');
const { Loader } = require('sdk/test/loader');
const { setTimeout } = require('sdk/timers');

const root = PrefsTarget();

exports.testPrefsTarget = function(assert, done) {
  let loader = Loader(module);
  let pt = loader.require('sdk/preferences/event-target').PrefsTarget({});
  let name = 'test';

  assert.equal(get(name, ''), '', 'test pref is blank');

  pt.once(name, function() {
    assert.equal(pt.prefs[name], 2, 'test pref is 2');

    pt.once(name, function() {
      assert.fail('should not have heard a pref change');
    });
    loader.unload();
    root.once(name, function() {
      assert.pass('test pref was changed');
      reset(name);

      // NOTE: using setTimeout to make sure that the other listener had
      //       a chance to fail
      // end test
      setTimeout(done);
    });
    set(name, 3);
  });

  pt.prefs[name] = 2;
};

require('sdk/test').run(exports);
