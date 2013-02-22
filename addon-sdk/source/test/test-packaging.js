/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

var options = require("@loader/options");

exports.testPackaging = function(test) {
  test.assertEqual(options.metadata.description,
                   "Add-on development made easy.",
                   "packaging metadata should be available");
  try {
    options.metadata.description = 'new description';
    test.fail('should not have been able to set options.metadata property');
  }
  catch (e) {}

  test.assertEqual(options.metadata.description,
                   "Add-on development made easy.",
                   "packaging metadata should be frozen");

  test.assertEqual(options.metadata.permissions['private-browsing'], undefined,
                   "private browsing metadata should be undefined");
  test.assertEqual(options.metadata['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");
  test.assertEqual(options['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");

  try {
    options.metadata['private-browsing'] = true;
    test.fail('should not have been able to set options.metadata property');
  }
  catch(e) {}
  test.assertEqual(options.metadata['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");

  try {
    options.metadata.permissions['private-browsing'] = true;
    test.fail('should not have been able to set options.metadata.permissions property');
  }
  catch (e) {}
  test.assertEqual(options.metadata.permissions['private-browsing'], undefined,
                   "private browsing metadata should be be frozen");
};
