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
   * Renders a Infinity object
   */

  InfinityRep.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function InfinityRep(props) {
    const { object, shouldRenderTooltip } = props;

    const config = getElementConfig(shouldRenderTooltip, object);

    return span(config, object.type);
  }

  function getElementConfig(shouldRenderTooltip, object) {
    return {
      className: "objectBox objectBox-number",
      title: shouldRenderTooltip ? object.type : null,
    };
  }

  function supportsObject(object, noGrip = false) {
    const type = getGripType(object, noGrip);
    return type == "Infinity" || type == "-Infinity";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(InfinityRep),
    supportsObject,
  };
});
