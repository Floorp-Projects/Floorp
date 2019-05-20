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

const { MODE } = require("./constants");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a grip representing a window.
 */
WindowRep.propTypes = {
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  object: PropTypes.object.isRequired,
};

function WindowRep(props) {
  const { mode, object } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-Window",
  };

  if (mode === MODE.TINY) {
    return span(config, getTitle(object));
  }

  return span(
    config,
    getTitle(object, true),
    span({ className: "location" }, getLocation(object))
  );
}

function getTitle(object, trailingSpace) {
  let title = object.displayClass || object.class || "Window";
  if (trailingSpace === true) {
    title = `${title} `;
  }
  return span({ className: "objectTitle" }, title);
}

function getLocation(object) {
  return getURLDisplayString(object.preview.url);
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return object.preview && getGripType(object, noGrip) == "Window";
}

// Exports from this module
module.exports = {
  rep: wrapRender(WindowRep),
  supportsObject,
};
