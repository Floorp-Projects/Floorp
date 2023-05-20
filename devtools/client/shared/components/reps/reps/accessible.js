/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";
// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const {
    button,
    span,
  } = require("devtools/client/shared/vendor/react-dom-factories");

  // Utils
  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    rep: StringRep,
  } = require("devtools/client/shared/components/reps/reps/string");

  /**
   * Renders Accessible object.
   */

  Accessible.propTypes = {
    object: PropTypes.object.isRequired,
    inspectIconTitle: PropTypes.string,
    nameMaxLength: PropTypes.number,
    onAccessibleClick: PropTypes.func,
    onAccessibleMouseOver: PropTypes.func,
    onAccessibleMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    roleFirst: PropTypes.bool,
    separatorText: PropTypes.string,
    shouldRenderTooltip: PropTypes.bool,
  };

  function Accessible(props) {
    const {
      object,
      inspectIconTitle,
      nameMaxLength,
      onAccessibleClick,
      onInspectIconClick,
      roleFirst,
      separatorText,
    } = props;

    const isInTree = object.preview && object.preview.isConnected === true;

    const config = getElementConfig({ ...props, isInTree });
    const elements = getElements(
      object,
      nameMaxLength,
      roleFirst,
      separatorText
    );
    const inspectIcon = getInspectIcon({
      object,
      onInspectIconClick,
      inspectIconTitle,
      onAccessibleClick,
      isInTree,
    });

    return span(config, ...elements, inspectIcon);
  }

  // Get React Config Obj
  function getElementConfig(opts) {
    const {
      object,
      isInTree,
      onAccessibleClick,
      onAccessibleMouseOver,
      onAccessibleMouseOut,
      shouldRenderTooltip,
      roleFirst,
    } = opts;
    const { name, role } = object.preview;

    // Initiate config
    const config = {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-accessible",
    };

    if (isInTree) {
      if (onAccessibleClick) {
        Object.assign(config, {
          onClick: _ => onAccessibleClick(object),
          className: `${config.className} clickable`,
        });
      }

      if (onAccessibleMouseOver) {
        Object.assign(config, {
          onMouseOver: _ => onAccessibleMouseOver(object),
        });
      }

      if (onAccessibleMouseOut) {
        Object.assign(config, {
          onMouseOut: onAccessibleMouseOut,
        });
      }
    }

    // If tooltip, build tooltip
    if (shouldRenderTooltip) {
      let tooltip;
      if (!name) {
        tooltip = role;
      } else {
        const quotedName = `"${name}"`;
        tooltip = `${roleFirst ? role : quotedName}: ${
          roleFirst ? quotedName : role
        }`;
      }

      config.title = tooltip;
    }

    // Return config obj
    return config;
  }

  // Get Content Elements
  function getElements(
    grip,
    nameMaxLength,
    roleFirst = false,
    separatorText = ": "
  ) {
    const { name, role } = grip.preview;
    const elements = [];

    // If there's a `name` value in `grip.preview`, render it with the
    // StringRep and push element into Elements array

    if (name) {
      elements.push(
        StringRep({
          className: "accessible-name",
          object: name,
          cropLimit: nameMaxLength,
        }),
        span({ className: "separator" }, separatorText)
      );
    }

    elements.push(span({ className: "accessible-role" }, role));
    return roleFirst ? elements.reverse() : elements;
  }

  // Get Icon
  function getInspectIcon(opts) {
    const {
      object,
      onInspectIconClick,
      inspectIconTitle,
      onAccessibleClick,
      isInTree,
    } = opts;

    if (!isInTree || !onInspectIconClick) {
      return null;
    }

    return button({
      className: "open-accessibility-inspector",
      title: inspectIconTitle,
      onClick: e => {
        if (onAccessibleClick) {
          e.stopPropagation();
        }

        onInspectIconClick(object, e);
      },
    });
  }

  // Registration
  function supportsObject(object) {
    return (
      object?.preview && object.typeName && object.typeName === "accessible"
    );
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(Accessible),
    supportsObject,
  };
});
