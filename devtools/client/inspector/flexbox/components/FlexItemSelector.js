/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  getSelectorFromGrip,
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");

const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;
const ElementNode = REPS.ElementNode;

const Types = require("../types");

loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);

class FlexItemSelector extends PureComponent {
  static get propTypes() {
    return {
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
      flexItems: PropTypes.arrayOf(PropTypes.shape(Types.flexItem)).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowFlexItemMenu = this.onShowFlexItemMenu.bind(this);
  }

  onShowFlexItemMenu(event) {
    event.stopPropagation();

    const { flexItem, flexItems, setSelectedNode } = this.props;
    const menuItems = [];

    for (const item of flexItems) {
      const grip = translateNodeFrontToGrip(item.nodeFront);
      menuItems.push({
        label: getSelectorFromGrip(grip),
        type: "checkbox",
        checked: item === flexItem,
        click: () => setSelectedNode(item.nodeFront),
      });
    }

    showMenu(menuItems, {
      button: event.target,
    });
  }

  render() {
    const { flexItem } = this.props;

    return dom.button(
      {
        id: "flex-item-selector",
        className: "devtools-button devtools-dropdown-button",
        onClick: this.onShowFlexItemMenu,
      },
      Rep({
        defaultRep: ElementNode,
        mode: MODE.TINY,
        object: translateNodeFrontToGrip(flexItem.nodeFront),
      })
    );
  }
}

module.exports = FlexItemSelector;
