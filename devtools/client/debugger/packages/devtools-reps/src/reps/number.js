/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");

const { getGripType, wrapRender } = require("./rep-utils");

/**
 * Renders a number
 */
Number.propTypes = {
  object: PropTypes.oneOfType([
    PropTypes.object,
    PropTypes.number,
    PropTypes.bool,
  ]).isRequired,
};

function Number(props) {
  const value = props.object;

  return span({ className: "objectBox objectBox-number" }, stringify(value));
}

function stringify(object) {
  const isNegativeZero =
    Object.is(object, -0) || (object.type && object.type == "-0");

  return isNegativeZero ? "-0" : String(object);
}

function supportsObject(object, noGrip = false) {
  return ["boolean", "number", "-0"].includes(getGripType(object, noGrip));
}

// Exports from this module

module.exports = {
  rep: wrapRender(Number),
  supportsObject,
};
