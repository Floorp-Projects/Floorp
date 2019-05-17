/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");

const { getGripType, wrapRender } = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a number
 */
BigInt.propTypes = {
  object: PropTypes.oneOfType([
    PropTypes.object,
    PropTypes.number,
    PropTypes.bool,
  ]).isRequired,
};

function BigInt(props) {
  const { text } = props.object;

  return span({ className: "objectBox objectBox-number" }, `${text}n`);
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) === "BigInt";
}

// Exports from this module

module.exports = {
  rep: wrapRender(BigInt),
  supportsObject,
};
