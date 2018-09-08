/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { translateNodeFrontToGrip } = require("devtools/client/inspector/shared/utils");

// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("../types");

loader.lazyRequireGetter(this, "showMenu", "devtools/client/shared/components/menu/utils", true);

class FlexItemSelector extends PureComponent {
  static get propTypes() {
    return {
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
      flexItems: PropTypes.arrayOf(PropTypes.shape(Types.flexItem)).isRequired,
      onToggleFlexItemShown: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowFlexItemMenu = this.onShowFlexItemMenu.bind(this);
  }

  onShowFlexItemMenu(event) {
    const {
      flexItem,
      flexItems,
      onToggleFlexItemShown,
    } = this.props;
    const menuItems = [];

    for (const item of flexItems) {
      const grip = translateNodeFrontToGrip(item.nodeFront);
      menuItems.push({
        label: getLabel(grip),
        type: "checkbox",
        checked: item === flexItem,
        click: () => onToggleFlexItemShown(item.nodeFront),
      });
    }

    showMenu(menuItems, {
      button: event.target,
    });
  }

  render() {
    const { flexItem } = this.props;

    return (
      dom.div({ className: "flex-item-selector-wrapper" },
        dom.button(
          {
            id: "flex-item-selector",
            className: "devtools-button devtools-dropdown-button",
            onClick: this.onShowFlexItemMenu,
          },
          Rep(
            {
              defaultRep: ElementNode,
              mode: MODE.TINY,
              object: translateNodeFrontToGrip(flexItem.nodeFront)
            }
          )
        )
      )
    );
  }
}

/**
 * Returns a selector label of the Element Rep from the grip. This is based on the
 * getElements() function in our devtools-reps component for a ElementNode.
 *
 * @param  {Object} grip
 *         Grip-like object that can be used with Reps.
 * @return {String} selector label of the element node.
 */
function getLabel(grip) {
  const {
    attributes,
    nodeName,
    isAfterPseudoElement,
    isBeforePseudoElement
  } = grip.preview;

  if (isAfterPseudoElement || isBeforePseudoElement) {
    return `::${isAfterPseudoElement ? "after" : "before"}`;
  }

  let label = nodeName;

  if (attributes.id) {
    label += `#${attributes.id}`;
  }

  if (attributes.class) {
    label += attributes.class
      .trim()
      .split(/\s+/)
      .map(cls => `.${cls}`)
      .join("");
  }

  return label;
}

module.exports = FlexItemSelector;
