/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

module.exports = createClass({
  displayName: "PanelMenuEntry",

  propTypes: {
    icon: PropTypes.string.isRequired,
    id: PropTypes.string.isRequired,
    name: PropTypes.string.isRequired,
    selected: PropTypes.bool,
    selectPanel: PropTypes.func.isRequired
  },

  onClick() {
    this.props.selectPanel(this.props.id);
  },

  onKeyDown(event) {
    if ([" ", "Enter"].includes(event.key)) {
      this.props.selectPanel(this.props.id);
    }
  },

  render() {
    let { id, name, icon, selected } = this.props;

    // Here .category, .category-icon, .category-name classnames are used to
    // apply common styles defined.
    let className = "category" + (selected ? " selected" : "");
    return dom.div({
      "aria-selected": selected,
      "aria-controls": id + "-panel",
      className,
      onClick: this.onClick,
      onKeyDown: this.onKeyDown,
      tabIndex: "0",
      role: "tab" },
    dom.img({ className: "category-icon", src: icon, role: "presentation" }),
    dom.div({ className: "category-name" }, name));
  }
});
