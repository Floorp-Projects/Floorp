/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var options = require("@loader/options");

exports.testPackaging = function(assert) {
  assert.equal(options.metadata.description,
                   "Add-on development made easy.",
                   "packaging metadata should be available");
  try {
    options.metadata.description = 'new description';
    assert.fail('should not have been able to set options.metadata property');
  }
  catch (e) {}

  assert.equal(options.metadata.description,
                   "Add-on development made easy.",
                   "packaging metadata should be frozen");

  assert.equal(options.metadata.permissions['private-browsing'], undefined,
                   "private browsing metadata should be undefined");
  assert.equal(options.metadata['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");
  assert.equal(options['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");

  try {
    options.metadata['private-browsing'] = true;
    assert.fail('should not have been able to set options.metadata property');
  }
  catch(e) {}
  assert.equal(options.metadata['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");

  try {
    options.metadata.permissions['private-browsing'] = true;
    assert.fail('should not have been able to set options.metadata.permissions property');
  }
  catch (e) {}
  assert.equal(options.metadata.permissions['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");
};

require("sdk/test/runner").runTestsFromModule(module);
