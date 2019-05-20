/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
const { span } = dom;

const { wrapRender } = require("./rep-utils");

/**
 * Renders a caption. This template is used by other components
 * that needs to distinguish between a simple text/value and a label.
 */
Caption.propTypes = {
  object: PropTypes.oneOfType([PropTypes.number, PropTypes.string]).isRequired,
};

function Caption(props) {
  return span({ className: "caption" }, props.object);
}

// Exports from this module
module.exports = wrapRender(Caption);
