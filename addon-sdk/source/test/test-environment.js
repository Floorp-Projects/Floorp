/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { env } = require('sdk/system/environment');
const { Cc, Ci } = require('chrome');
const { get, set, exists } = Cc['@mozilla.org/process/environment;1'].
                             getService(Ci.nsIEnvironment);

exports['test exists'] = function(assert) {
  assert.equal('PATH' in env, exists('PATH'),
               'PATH environment variable is defined');
  assert.equal('FOO1' in env, exists('FOO1'),
               'FOO1 environment variable is not defined');
  set('FOO1', 'foo');
  assert.equal('FOO1' in env, true,
               'FOO1 environment variable was set');
  set('FOO1', null);
  assert.equal('FOO1' in env, false,
               'FOO1 environment variable was unset');
};

exports['test get'] = function(assert) {
  assert.equal(env.PATH, get('PATH'), 'PATH env variable matches');
  assert.equal(env.BAR2, undefined, 'BAR2 env variable is not defined');
  set('BAR2', 'bar');
  assert.equal(env.BAR2, 'bar', 'BAR2 env variable was set');
  set('BAR2', null);
  assert.equal(env.BAR2, undefined, 'BAR2 env variable was unset');
};

exports['test set'] = function(assert) {
  assert.equal(get('BAZ3'), '', 'BAZ3 env variable is not set');
  assert.equal(env.BAZ3, undefined, 'BAZ3 is not set');
  env.BAZ3 = 'baz';
  assert.equal(env.BAZ3, get('BAZ3'), 'BAZ3 env variable is set');
  assert.equal(get('BAZ3'), 'baz', 'BAZ3 env variable was set to "baz"');
};

exports['test unset'] = function(assert) {
  env.BLA4 = 'bla';
  assert.equal(env.BLA4, 'bla', 'BLA4 env varibale is set');
  delete env.BLA4;
  assert.equal(env.BLA4, undefined, 'BLA4 env variable is unset');
  assert.equal('BLA4' in env, false, 'BLA4 env variable no longer exists' );
};

require('test').run(exports);
