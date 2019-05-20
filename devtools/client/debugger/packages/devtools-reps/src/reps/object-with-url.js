/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");

// Reps
const { isGrip, getURLDisplayString, wrapRender } = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a grip object with URL data.
 */
ObjectWithURL.propTypes = {
  object: PropTypes.object.isRequired,
};

function ObjectWithURL(props) {
  const grip = props.object;
  return span(
    {
      "data-link-actor-id": grip.actor,
      className: `objectBox objectBox-${getType(grip)}`,
    },
    getTitle(grip),
    span({ className: "objectPropValue" }, getDescription(grip))
  );
}

function getTitle(grip) {
  return span({ className: "objectTitle" }, `${getType(grip)} `);
}

function getType(grip) {
  return grip.class;
}

function getDescription(grip) {
  return getURLDisplayString(grip.preview.url);
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.preview.kind == "ObjectWithURL";
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectWithURL),
  supportsObject,
};
