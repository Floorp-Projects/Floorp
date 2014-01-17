/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

try {
  require('./not-found');
}
catch (e1) {
  exports.firstError = e1;
  // It should throw again and not be cached
  try {
    require('./not-found');
  }
  catch (e2) {
    exports.secondError = e2;
  }
}

try {
  require('./file.json');
}
catch (e) {
  exports.invalidJSON1 = e;
  try {
    require('./file.json');
  }
  catch (e) {
    exports.invalidJSON2 = e;
  }
}
