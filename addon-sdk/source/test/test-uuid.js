/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { uuid } = require('sdk/util/uuid');

exports['test generate uuid'] = function(assert) {
  let signature = /{[0-9a-f\-]+}/
  let first = String(uuid());
  let second = String(uuid());

  assert.ok(signature.test(first), 'first guid has a correct signature');
  assert.ok(signature.test(second), 'second guid has a correct signature');
  assert.notEqual(first, second, 'guid generates new guid on each call');
};

exports['test parse uuid'] = function(assert) {
  let firefoxUUID = '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}';
  let actual = uuid(firefoxUUID);

  assert.equal(actual.number, firefoxUUID, 'uuid parsed given string');
  assert.equal(String(actual), firefoxUUID, 'serializes to the same value');
};

require('sdk/test').run(exports);
