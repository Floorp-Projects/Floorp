/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  const {
    getGripType,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders a number
   */

  Number.propTypes = {
    object: PropTypes.oneOfType([
      PropTypes.object,
      PropTypes.number,
      PropTypes.bool,
    ]).isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function Number(props) {
    const value = stringify(props.object);
    const config = getElementConfig(props.shouldRenderTooltip, value);

    return span(config, value);
  }

  function stringify(object) {
    const isNegativeZero =
      Object.is(object, -0) || (object.type && object.type == "-0");

    return isNegativeZero ? "-0" : String(object);
  }

  function getElementConfig(shouldRenderTooltip, value) {
    return {
      className: "objectBox objectBox-number",
      title: shouldRenderTooltip ? value : null,
    };
  }

  const SUPPORTED_TYPES = new Set(["boolean", "number", "-0"]);
  function supportsObject(object, noGrip = false) {
    return SUPPORTED_TYPES.has(getGripType(object, noGrip));
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(Number),
    supportsObject,
  };
});
