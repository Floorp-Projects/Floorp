/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  PAGE_TYPES,
} = require("resource://devtools/client/application/src/constants.js");

const page = PropTypes.oneOf(Object.values(PAGE_TYPES));

module.exports = {
  page,
};
