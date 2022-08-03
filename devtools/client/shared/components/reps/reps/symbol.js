/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Dependencies
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  const {
    getGripType,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const {
    rep: StringRep,
  } = require("devtools/client/shared/components/reps/reps/string");

  const MAX_STRING_LENGTH = 50;

  /**
   * Renders a symbol.
   */

  SymbolRep.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function SymbolRep(props) {
    const {
      className = "objectBox objectBox-symbol",
      object,
      shouldRenderTooltip,
    } = props;
    const { name } = object;

    let symbolText = name || "";
    if (name && name !== "Symbol.iterator" && name !== "Symbol.asyncIterator") {
      symbolText = StringRep({
        object: symbolText,
        shouldCrop: true,
        cropLimit: MAX_STRING_LENGTH,
        useQuotes: true,
      });
    }

    const config = getElementConfig(
      {
        shouldRenderTooltip,
        className,
        name,
      },
      object
    );

    return span(config, "Symbol(", symbolText, ")");
  }

  function getElementConfig(opts, object) {
    const { shouldRenderTooltip, className, name } = opts;

    return {
      "data-link-actor-id": object.actor,
      className,
      title: shouldRenderTooltip ? `Symbol(${name})` : null,
    };
  }

  function supportsObject(object, noGrip = false) {
    return getGripType(object, noGrip) == "symbol";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(SymbolRep),
    supportsObject,
  };
});
