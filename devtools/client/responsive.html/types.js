/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PropTypes } = require("devtools/client/shared/vendor/react");

// React PropTypes are used to describe the expected "shape" of various common
// objects that get passed down as props to components.

/**
 * A single viewport displaying a document.
 */
exports.viewport = {

  // The id of the viewport
  id: PropTypes.number.isRequired,

  // The width of the viewport
  width: PropTypes.number,

  // The height of the viewport
  height: PropTypes.number,

};

/**
 * The location of the document displayed in the viewport(s).
 */
exports.location = PropTypes.string;
