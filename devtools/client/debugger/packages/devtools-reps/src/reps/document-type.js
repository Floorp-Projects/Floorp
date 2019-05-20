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
 * Renders DOM documentType object.
 */
DocumentType.propTypes = {
  object: PropTypes.object.isRequired,
};

function DocumentType(props) {
  const { object } = props;
  const name =
    object && object.preview && object.preview.nodeName
      ? ` ${object.preview.nodeName}`
      : "";
  return span(
    {
      "data-link-actor-id": props.object.actor,
      className: "objectBox objectBox-document",
    },
    `<!DOCTYPE${name}>`
  );
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  const type = getGripType(object, noGrip);
  return object.preview && type === "DocumentType";
}

// Exports from this module
module.exports = {
  rep: wrapRender(DocumentType),
  supportsObject,
};
