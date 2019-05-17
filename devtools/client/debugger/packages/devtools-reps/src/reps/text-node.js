/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");

// Reps
const { isGrip, cropString, wrapRender } = require("./rep-utils");
const { MODE } = require("./constants");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders DOM #text node.
 */
TextNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
};

function TextNode(props) {
  const {
    object: grip,
    mode = MODE.SHORT,
    onDOMNodeMouseOver,
    onDOMNodeMouseOut,
    onInspectIconClick,
  } = props;

  const baseConfig = {
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-textNode",
  };
  let inspectIcon;
  const isInTree = grip.preview && grip.preview.isConnected === true;

  if (isInTree) {
    if (onDOMNodeMouseOver) {
      Object.assign(baseConfig, {
        onMouseOver: _ => onDOMNodeMouseOver(grip),
      });
    }

    if (onDOMNodeMouseOut) {
      Object.assign(baseConfig, {
        onMouseOut: onDOMNodeMouseOut,
      });
    }

    if (onInspectIconClick) {
      inspectIcon = dom.button({
        className: "open-inspector",
        draggable: false,
        // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
        title: "Click to select the node in the inspector",
        onClick: e => onInspectIconClick(grip, e),
      });
    }
  }

  if (mode === MODE.TINY) {
    return span(baseConfig, getTitle(grip), inspectIcon);
  }

  return span(
    baseConfig,
    getTitle(grip),
    span({ className: "nodeValue" }, " ", `"${getTextContent(grip)}"`),
    inspectIcon
  );
}

function getTextContent(grip) {
  return cropString(grip.preview.textContent);
}

function getTitle(grip) {
  const title = "#text";
  return span({}, title);
}

// Registration
function supportsObject(grip, noGrip = false) {
  if (noGrip === true || !isGrip(grip)) {
    return false;
  }

  return grip.preview && grip.class == "Text";
}

// Exports from this module
module.exports = {
  rep: wrapRender(TextNode),
  supportsObject,
};
