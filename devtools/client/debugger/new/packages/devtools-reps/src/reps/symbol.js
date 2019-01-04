/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");

const { getGripType, wrapRender } = require("./rep-utils");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders a symbol.
 */
SymbolRep.propTypes = {
  object: PropTypes.object.isRequired
};

function SymbolRep(props) {
  const { className = "objectBox objectBox-symbol", object } = props;
  const { name } = object;

  return span(
    {
      className,
      "data-link-actor-id": object.actor
    },
    `Symbol(${name || ""})`
  );
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "symbol";
}

// Exports from this module
module.exports = {
  rep: wrapRender(SymbolRep),
  supportsObject
};
