/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


define(function () {
  // This is wrong and should not be allowed.
  define('dupeNested2', {
      name: 'dupeNested2'
  });

  return {
    name: 'dupeNested'
  };
});
