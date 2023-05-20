/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  // Reps
  const {
    getGripType,
    wrapRender,
    ELLIPSIS,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders a grip object with regular expression.
   */

  RegExp.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function RegExp(props) {
    const { object } = props;
    const config = getElementConfig(props);

    return span(config, getSource(object));
  }

  function getElementConfig(opts) {
    const { object, shouldRenderTooltip } = opts;
    const text = getSource(object);

    return {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-regexp regexpSource",
      title: shouldRenderTooltip ? text : null,
    };
  }

  function getSource(grip) {
    const { displayString } = grip;
    if (displayString?.type === "longString") {
      return `${displayString.initial}${ELLIPSIS}`;
    }

    return displayString;
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    return getGripType(object, noGrip) == "RegExp";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(RegExp),
    supportsObject,
  };
});
