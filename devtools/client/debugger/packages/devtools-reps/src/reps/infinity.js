/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");

const { getGripType, wrapRender } = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a Infinity object
 */
InfinityRep.propTypes = {
  object: PropTypes.object.isRequired,
};

function InfinityRep(props) {
  const { object } = props;

  return span({ className: "objectBox objectBox-number" }, object.type);
}

function supportsObject(object, noGrip = false) {
  const type = getGripType(object, noGrip);
  return type == "Infinity" || type == "-Infinity";
}

// Exports from this module
module.exports = {
  rep: wrapRender(InfinityRep),
  supportsObject,
};
