/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");
const { button, span } = require("react-dom-factories");

// Utils
const { isGrip, wrapRender } = require("./rep-utils");
const { rep: StringRep } = require("./string");

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
};

function Accessible(props) {
  const {
    object,
    inspectIconTitle,
    nameMaxLength,
    onAccessibleClick,
    onAccessibleMouseOver,
    onAccessibleMouseOut,
    onInspectIconClick,
    roleFirst,
    separatorText,
  } = props;
  const elements = getElements(object, nameMaxLength, roleFirst, separatorText);
  const isInTree = object.preview && object.preview.isConnected === true;
  const baseConfig = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-accessible",
  };

  let inspectIcon;
  if (isInTree) {
    if (onAccessibleClick) {
      Object.assign(baseConfig, {
        onClick: _ => onAccessibleClick(object),
        className: `${baseConfig.className} clickable`,
      });
    }

    if (onAccessibleMouseOver) {
      Object.assign(baseConfig, {
        onMouseOver: _ => onAccessibleMouseOver(object),
      });
    }

    if (onAccessibleMouseOut) {
      Object.assign(baseConfig, {
        onMouseOut: onAccessibleMouseOut,
      });
    }

    if (onInspectIconClick) {
      inspectIcon = button({
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
  }

  return span(baseConfig, ...elements, inspectIcon);
}

function getElements(
  grip,
  nameMaxLength,
  roleFirst = false,
  separatorText = ": "
) {
  const { name, role } = grip.preview;
  const elements = [];
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

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  return object.preview && object.typeName && object.typeName === "accessible";
}

// Exports from this module
module.exports = {
  rep: wrapRender(Accessible),
  supportsObject,
};
