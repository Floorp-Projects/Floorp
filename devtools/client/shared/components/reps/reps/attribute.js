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
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    rep: StringRep,
  } = require("devtools/client/shared/components/reps/reps/string");

  /**
   * Renders DOM attribute
   */

  Attribute.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function Attribute(props) {
    const { object, shouldRenderTooltip } = props;
    const value = object.preview.value;
    const attrName = getTitle(object);

    const config = getElementConfig({
      attrName,
      shouldRenderTooltip,
      value,
      object,
    });

    return span(
      config,
      span({ className: "attrName" }, attrName),
      span({ className: "attrEqual" }, "="),
      StringRep({ className: "attrValue", object: value })
    );
  }

  function getTitle(grip) {
    return grip.preview.nodeName;
  }

  function getElementConfig(opts) {
    const { attrName, shouldRenderTooltip, value, object } = opts;

    return {
      "data-link-actor-id": object.actor,
      className: "objectBox-Attr",
      title: shouldRenderTooltip ? `${attrName}="${value}"` : null,
    };
  }

  // Registration
  function supportsObject(grip, noGrip = false) {
    return getGripType(grip, noGrip) == "Attr" && grip?.preview;
  }

  module.exports = {
    rep: wrapRender(Attribute),
    supportsObject,
  };
});
