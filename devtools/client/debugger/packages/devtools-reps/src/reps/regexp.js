/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");

// Reps
const { getGripType, isGrip, wrapRender } = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a grip object with regular expression.
 */
RegExp.propTypes = {
  object: PropTypes.object.isRequired,
};

function RegExp(props) {
  const { object } = props;

  return span(
    {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-regexp regexpSource",
    },
    getSource(object)
  );
}

function getSource(grip) {
  return grip.displayString;
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return getGripType(object, noGrip) == "RegExp";
}

// Exports from this module
module.exports = {
  rep: wrapRender(RegExp),
  supportsObject,
};
