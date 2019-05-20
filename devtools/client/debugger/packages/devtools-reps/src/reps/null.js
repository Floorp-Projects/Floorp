/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const { wrapRender } = require("./rep-utils");
const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders null value
 */
function Null(props) {
  return span({ className: "objectBox objectBox-null" }, "null");
}

function supportsObject(object, noGrip = false) {
  if (noGrip === true) {
    return object === null;
  }

  if (object && object.type && object.type == "null") {
    return true;
  }

  return object == null;
}

// Exports from this module

module.exports = {
  rep: wrapRender(Null),
  supportsObject,
};
