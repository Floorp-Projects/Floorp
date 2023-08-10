/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  // Dependencies
  const {
    getGripType,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const Grip = require("devtools/client/shared/components/reps/reps/grip");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  /**
   * Renders a DOM Promise object.
   */

  PromiseRep.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function PromiseRep(props) {
    const object = props.object;

    // @backward-compat { version 85 } On older servers, the preview of a promise was
    // useless and didn't include the internal promise state, which was directly exposed
    // in the grip.
    if (object.promiseState) {
      const { state, value, reason } = object.promiseState;
      const ownProperties = Object.create(null);
      ownProperties["<state>"] = { value: state };
      let ownPropertiesLength = 1;
      if (state == "fulfilled") {
        ownProperties["<value>"] = { value };
        ++ownPropertiesLength;
      } else if (state == "rejected") {
        ownProperties["<reason>"] = { value: reason };
        ++ownPropertiesLength;
      }
      object.preview = {
        kind: "Object",
        ownProperties,
        ownPropertiesLength,
      };
    }

    if (props.mode !== MODE.TINY && props.mode !== MODE.HEADER) {
      return Grip.rep(props);
    }

    const shouldRenderTooltip = props.shouldRenderTooltip;
    const config = {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-object",
      title: shouldRenderTooltip ? "Promise" : null,
    };

    if (props.mode === MODE.HEADER) {
      return span(config, getTitle(object));
    }

    const { Rep } = require("devtools/client/shared/components/reps/reps/rep");

    return span(
      config,
      getTitle(object),
      span({ className: "objectLeftBrace" }, " { "),
      Rep({ object: object.preview.ownProperties["<state>"].value }),
      span({ className: "objectRightBrace" }, " }")
    );
  }

  function getTitle(object) {
    return span({ className: "objectTitle" }, object.class);
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    if (!Grip.supportsObject(object, noGrip)) {
      return false;
    }
    return getGripType(object, noGrip) == "Promise";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(PromiseRep),
    supportsObject,
  };
});
