/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { isPrivateBrowsingSupported } = require('sdk/self');

exports.testIsPrivateBrowsingTrue = function(assert) {
  assert.ok(isPrivateBrowsingSupported,
            'isPrivateBrowsingSupported property is true');
};

require('sdk/test/runner').runTestsFromModule(module);
