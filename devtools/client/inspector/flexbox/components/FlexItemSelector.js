/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getSelectorFromGrip,
  translateNodeFrontToGrip,
} = require("resource://devtools/client/inspector/shared/utils.js");

loader.lazyRequireGetter(
  this,
  "getNodeRep",
  "resource://devtools/client/inspector/shared/node-reps.js"
);

const Types = require("resource://devtools/client/inspector/flexbox/types.js");

loader.lazyRequireGetter(
  this,
  "showMenu",
  "resource://devtools/client/shared/components/menu/utils.js",
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
      getNodeRep(flexItem.nodeFront)
    );
  }
}

module.exports = FlexItemSelector;
