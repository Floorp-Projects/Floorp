/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CHANGE_LOCATION } = require("./index");

module.exports = {

  /**
   * The location of the page has changed.  This may be triggered by the user
   * directly entering a new URL, navigating with links, etc.
   */
  changeLocation(location) {
    return {
      type: CHANGE_LOCATION,
      location,
    };
  },

};
