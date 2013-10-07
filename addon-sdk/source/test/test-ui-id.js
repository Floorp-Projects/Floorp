/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { identify } = require('sdk/ui/id');
const { Class } = require('sdk/core/heritage');

const signature = /{[0-9a-f\-]+}/;

exports['test generate id'] = function(assert) {
  let first = identify({});
  let second = identify({});

  assert.ok(signature.test(first), 'first id has a correct signature');
  assert.ok(signature.test(second), 'second id has a correct signature');

  assert.notEqual(first, identify({}), 'identify generated new uuid [1]');
  assert.notEqual(first, second, 'identify generated new uuid [2]');
};

exports['test get id'] = function(assert) {
  let thing = {};
  let thingID = identify(thing);

  assert.equal(thingID, identify(thing), 'ids for things are cached by default');
  assert.notEqual(identify(thing), identify({}), 'new ids for new things');
};

exports['test custom id definition for classes'] = function(assert) {
  let Thing = Class({});
  let thing = Thing();
  let counter = 0;

  identify.define(Thing, function(thing) {
    return ++counter;
  });

  assert.equal(identify(thing), counter, 'it is possible to define custom identifications');
  assert.ok(signature.test(identify({})), 'defining a custom identification does not affect the default behavior');
}

require('sdk/test').run(exports);
