/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// @TODO Load the actual strings from accessibility.properties instead.
class L10N {
  getStr(str) {
    switch (str) {
      default:
        return str;
    }
  }

  getFormatStr(str) {
    return this.getStr(str);
  }
}

module.exports = {
  L10N: new L10N(),
};
