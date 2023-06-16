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
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const String =
    require("devtools/client/shared/components/reps/reps/string").rep;

  /**
   * Renders a grip object with textual data.
   */

  ObjectWithText.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function ObjectWithText(props) {
    const grip = props.object;
    const config = getElementConfig(props);

    return span(config, `${getType(grip)} `, getDescription(grip));
  }

  function getElementConfig(opts) {
    const shouldRenderTooltip = opts.shouldRenderTooltip;
    const grip = opts.object;

    return {
      "data-link-actor-id": grip.actor,
      className: `objectTitle objectBox objectBox-${getType(grip)}`,
      title: shouldRenderTooltip
        ? `${getType(grip)} "${grip.preview.text}"`
        : null,
    };
  }

  function getType(grip) {
    return grip.class;
  }

  function getDescription(grip) {
    return String({
      object: grip.preview.text,
    });
  }

  // Registration
  function supportsObject(grip) {
    return grip?.preview?.kind == "ObjectWithText";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(ObjectWithText),
    supportsObject,
  };
});
