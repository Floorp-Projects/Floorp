/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";
// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const {
    button,
    span,
  } = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  /**
   * Renders an object. An object is represented by a list of its
   * properties enclosed in curly brackets.
   */

  Accessor.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    shouldRenderTooltip: PropTypes.bool,
  };

  function Accessor(props) {
    const {
      object,
      evaluation,
      onInvokeGetterButtonClick,
      shouldRenderTooltip,
    } = props;

    if (evaluation) {
      const {
        Rep,
        Grip,
      } = require("devtools/client/shared/components/reps/reps/rep");
      return span(
        {
          className: "objectBox objectBox-accessor objectTitle",
        },
        Rep({
          ...props,
          object: evaluation.getterValue,
          mode: props.mode || MODE.TINY,
          defaultRep: Grip,
        })
      );
    }

    if (hasGetter(object) && onInvokeGetterButtonClick) {
      return button({
        className: "invoke-getter",
        title: "Invoke getter",
        onClick: event => {
          onInvokeGetterButtonClick();
          event.stopPropagation();
        },
      });
    }

    const accessors = [];
    if (hasGetter(object)) {
      accessors.push("Getter");
    }

    if (hasSetter(object)) {
      accessors.push("Setter");
    }

    const accessorsString = accessors.join(" & ");

    return span(
      {
        className: "objectBox objectBox-accessor objectTitle",
        title: shouldRenderTooltip ? accessorsString : null,
      },
      accessorsString
    );
  }

  function hasGetter(object) {
    return object && object.get && object.get.type !== "undefined";
  }

  function hasSetter(object) {
    return object && object.set && object.set.type !== "undefined";
  }

  function supportsObject(object) {
    return hasGetter(object) || hasSetter(object);
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(Accessor),
    supportsObject,
  };
});
