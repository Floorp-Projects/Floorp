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
   * Renders a NaN object
   */

  NaNRep.PropTypes = {
    shouldRenderTooltip: PropTypes.bool,
  };

  function NaNRep(props) {
    const shouldRenderTooltip = props.shouldRenderTooltip;

    const config = getElementConfig(shouldRenderTooltip);

    return span(config, "NaN");
  }

  function getElementConfig(shouldRenderTooltip) {
    return {
      className: "objectBox objectBox-nan",
      title: shouldRenderTooltip ? "NaN" : null,
    };
  }

  function supportsObject(object, noGrip = false) {
    return getGripType(object, noGrip) == "NaN";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(NaNRep),
    supportsObject,
  };
});
