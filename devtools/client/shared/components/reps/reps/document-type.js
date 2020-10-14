/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  // Reps
  const {
    getGripType,
    isGrip,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders DOM documentType object.
   */

  DocumentType.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function DocumentType(props) {
    const { object, shouldRenderTooltip } = props;
    const name =
      object && object.preview && object.preview.nodeName
        ? ` ${object.preview.nodeName}`
        : "";

    const config = getElementConfig({ object, shouldRenderTooltip, name });

    return span(config, `<!DOCTYPE${name}>`);
  }

  function getElementConfig(opts) {
    const { object, shouldRenderTooltip, name } = opts;

    return {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-document",
      title: shouldRenderTooltip ? `<!DOCTYPE${name}>` : null,
    };
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    if (noGrip === true || !isGrip(object)) {
      return false;
    }

    const type = getGripType(object, noGrip);
    return object.preview && type === "DocumentType";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(DocumentType),
    supportsObject,
  };
});
