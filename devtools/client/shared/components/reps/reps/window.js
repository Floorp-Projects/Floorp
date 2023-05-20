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

  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  /**
   * Renders a grip representing a window.
   */

  WindowRep.propTypes = {
    mode: PropTypes.oneOf(Object.values(MODE)),
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function WindowRep(props) {
    const { mode, object } = props;

    if (mode === MODE.TINY) {
      const tinyTitle = getTitle(object);
      const title = getTitle(object, true);
      const location = getLocation(object);
      const config = getElementConfig({ ...props, title, location });

      return span(
        config,
        span({ className: tinyTitle.className }, tinyTitle.content)
      );
    }

    const title = getTitle(object, true);
    const location = getLocation(object);
    const config = getElementConfig({ ...props, title, location });

    return span(
      config,
      span({ className: title.className }, title.content),
      span({ className: "location" }, location)
    );
  }

  function getElementConfig(opts) {
    const { object, shouldRenderTooltip, title, location } = opts;
    let tooltip;

    if (location) {
      tooltip = `${title.content}${location}`;
    } else {
      tooltip = `${title.content}`;
    }

    return {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-Window",
      title: shouldRenderTooltip ? tooltip : null,
    };
  }

  function getTitle(object, trailingSpace) {
    let title = object.displayClass || object.class || "Window";
    if (trailingSpace === true) {
      title = `${title} `;
    }
    return {
      className: "objectTitle",
      content: title,
    };
  }

  function getLocation(object) {
    return getURLDisplayString(object.preview.url);
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    return object?.preview && getGripType(object, noGrip) == "Window";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(WindowRep),
    supportsObject,
  };
});
