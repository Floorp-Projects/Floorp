/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");

// Reps
const {
  getGripType,
  isGrip,
  getURLDisplayString,
  wrapRender,
} = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders DOM document object.
 */
Document.propTypes = {
  object: PropTypes.object.isRequired,
};

function Document(props) {
  const grip = props.object;
  const location = getLocation(grip);
  return span(
    {
      "data-link-actor-id": grip.actor,
      className: "objectBox objectBox-document",
    },
    getTitle(grip),
    location ? span({ className: "location" }, ` ${location}`) : null
  );
}

function getLocation(grip) {
  const location = grip.preview.location;
  return location ? getURLDisplayString(location) : null;
}

function getTitle(grip) {
  return span(
    {
      className: "objectTitle",
    },
    grip.class
  );
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  const type = getGripType(object, noGrip);
  return object.preview && type === "HTMLDocument";
}

// Exports from this module
module.exports = {
  rep: wrapRender(Document),
  supportsObject,
};
