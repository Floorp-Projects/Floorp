/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
const { span } = dom;

// Reps
const { getGripType, isGrip, wrapRender } = require("./rep-utils");
const { rep: StringRep } = require("./string");

/**
 * Renders DOM attribute
 */
Attribute.propTypes = {
  object: PropTypes.object.isRequired,
};

function Attribute(props) {
  const { object } = props;
  const value = object.preview.value;

  return span(
    {
      "data-link-actor-id": object.actor,
      className: "objectBox-Attr",
    },
    span({ className: "attrName" }, getTitle(object)),
    span({ className: "attrEqual" }, "="),
    StringRep({ className: "attrValue", object: value, title: value })
  );
}

function getTitle(grip) {
  return grip.preview.nodeName;
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return getGripType(grip, noGrip) == "Attr" && grip.preview;
}

module.exports = {
  rep: wrapRender(Attribute),
  supportsObject,
};
