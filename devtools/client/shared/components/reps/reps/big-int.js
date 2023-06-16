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
   * Renders a BigInt Number
   */

  BigInt.propTypes = {
    object: PropTypes.oneOfType([
      PropTypes.object,
      PropTypes.number,
      PropTypes.bool,
    ]).isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function BigInt(props) {
    const { object, shouldRenderTooltip } = props;
    const text = object.text;
    const config = getElementConfig({ text, shouldRenderTooltip });

    return span(config, `${text}n`);
  }

  function getElementConfig(opts) {
    const { text, shouldRenderTooltip } = opts;

    return {
      className: "objectBox objectBox-number",
      title: shouldRenderTooltip ? `${text}n` : null,
    };
  }
  function supportsObject(object, noGrip = false) {
    return getGripType(object, noGrip) === "BigInt";
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(BigInt),
    supportsObject,
  };
});
