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
 * Used to render JS built-in Date() object.
 */
DateTime.propTypes = {
  object: PropTypes.object.isRequired,
};

function DateTime(props) {
  const grip = props.object;
  let date;
  try {
    const dateObject = new Date(grip.preview.timestamp);
    // Calling `toISOString` will throw if the date is invalid,
    // so we can render an `Invalid Date` element.
    dateObject.toISOString();

    date = span(
      {
        "data-link-actor-id": grip.actor,
        className: "objectBox",
      },
      getTitle(grip),
      span({ className: "Date" }, dateObject.toString())
    );
  } catch (e) {
    date = span({ className: "objectBox" }, "Invalid Date");
  }

  return date;
}

function getTitle(grip) {
  return span(
    {
      className: "objectTitle",
    },
    `${grip.class} `
  );
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return getGripType(grip, noGrip) == "Date" && grip.preview;
}

// Exports from this module
module.exports = {
  rep: wrapRender(DateTime),
  supportsObject,
};
