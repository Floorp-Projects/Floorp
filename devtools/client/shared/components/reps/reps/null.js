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
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders null value
   */

  Null.PropTypes = {
    shouldRenderTooltip: PropTypes.bool,
  };

  function Null(props) {
    const shouldRenderTooltip = props.shouldRenderTooltip;

    const config = getElementConfig(shouldRenderTooltip);

    return span(config, "null");
  }

  function getElementConfig(shouldRenderTooltip) {
    return {
      className: "objectBox objectBox-null",
      title: shouldRenderTooltip ? "null" : null,
    };
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
});
