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
    getURLDisplayString,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders a grip representing CSSStyleSheet
   */

  StyleSheet.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function StyleSheet(props) {
    const grip = props.object;
    const shouldRenderTooltip = props.shouldRenderTooltip;
    const location = getLocation(grip);
    const config = getElementConfig({ grip, shouldRenderTooltip, location });

    return span(
      config,
      getTitle(grip),
      span({ className: "objectPropValue" }, location)
    );
  }

  function getElementConfig(opts) {
    const { grip, shouldRenderTooltip, location } = opts;

    return {
      "data-link-actor-id": grip.actor,
      className: "objectBox objectBox-object",
      title: shouldRenderTooltip
        ? `${getGripType(grip, false)} ${location}`
        : null,
    };
  }

  function getTitle(grip) {
    return span(
      { className: "objectBoxTitle" },
      `${getGripType(grip, false)} `
    );
  }

  function getLocation(grip) {
    // Embedded stylesheets don't have URL and so, no preview.
    const url = grip.preview ? grip.preview.url : "";
    return url ? getURLDisplayString(url) : "";
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    return getGripType(object, noGrip) == "CSSStyleSheet";
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(StyleSheet),
    supportsObject,
  };
});
