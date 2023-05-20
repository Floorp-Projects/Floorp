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
    getURLDisplayString,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders a grip object with URL data.
   */

  ObjectWithURL.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function ObjectWithURL(props) {
    const grip = props.object;
    const config = getElementConfig(props);

    return span(
      config,
      getTitle(grip),
      span({ className: "objectPropValue" }, getDescription(grip))
    );
  }

  function getElementConfig(opts) {
    const grip = opts.object;
    const shouldRenderTooltip = opts.shouldRenderTooltip;
    const tooltip = `${getType(grip)} ${getDescription(grip)}`;

    return {
      "data-link-actor-id": grip.actor,
      className: `objectBox objectBox-${getType(grip)}`,
      title: shouldRenderTooltip ? tooltip : null,
    };
  }

  function getTitle(grip) {
    return span({ className: "objectTitle" }, `${getType(grip)} `);
  }

  function getType(grip) {
    return grip.class;
  }

  function getDescription(grip) {
    return getURLDisplayString(grip.preview.url);
  }

  // Registration
  function supportsObject(grip) {
    return grip?.preview?.kind == "ObjectWithURL";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(ObjectWithURL),
    supportsObject,
  };
});
