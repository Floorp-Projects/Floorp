/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  const {
    getGripType,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders undefined value
   */

  Undefined.propTypes = {
    shouldRenderTooltip: PropTypes.bool,
  };

  function Undefined(props) {
    const shouldRenderTooltip = props.shouldRenderTooltip;

    const config = getElementConfig(shouldRenderTooltip);

    return span(config, "undefined");
  }

  function getElementConfig(shouldRenderTooltip) {
    return {
      className: "objectBox objectBox-undefined",
      title: shouldRenderTooltip ? "undefined" : null,
    };
  }

  function supportsObject(object, noGrip = false) {
    if (noGrip === true) {
      return object === undefined;
    }

    return (
      (object && object.type && object.type == "undefined") ||
      getGripType(object, noGrip) == "undefined"
    );
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(Undefined),
    supportsObject,
  };
});
