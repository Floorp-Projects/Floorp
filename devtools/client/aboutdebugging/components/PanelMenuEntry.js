/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

class PanelMenuEntry extends Component {
  static get propTypes() {
    return {
      icon: PropTypes.string.isRequired,
      id: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired,
      selected: PropTypes.bool,
      selectPanel: PropTypes.func.isRequired
    };
  }

  constructor(props) {
    super(props);
    this.onClick = this.onClick.bind(this);
  }

  onClick() {
    this.props.selectPanel(this.props.id);
  }

  render() {
    const { id, name, icon, selected } = this.props;

    // Here .category, .category-icon, .category-name classnames are used to
    // apply common styles defined.
    const className = "category" + (selected ? " selected" : "");
    return dom.button({
      "aria-selected": selected,
      "aria-controls": id + "-panel",
      className,
      onClick: this.onClick,
      tabIndex: "0",
      role: "tab" },
    dom.img({ className: "category-icon", src: icon, role: "presentation" }),
    dom.div({ className: "category-name" }, name));
  }
}

module.exports = PanelMenuEntry;
