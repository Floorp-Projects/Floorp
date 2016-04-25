/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");

module.exports = createClass({
  displayName: "TabMenuEntry",

  onClick() {
    this.props.selectTab(this.props.tabId);
  },

  onKeyUp(event) {
    if ([" ", "Enter"].includes(event.key)) {
      this.props.selectTab(this.props.tabId);
    }
  },

  render() {
    let { panelId, icon, name, selected } = this.props;

    // Here .category, .category-icon, .category-name classnames are used to
    // apply common styles defined.
    let className = "category" + (selected ? " selected" : "");
    return dom.div({
      "aria-selected": selected,
      "aria-controls": panelId,
      className,
      onClick: this.onClick,
      onKeyUp: this.onKeyUp,
      tabIndex: "0",
      role: "tab" },
    dom.img({ className: "category-icon", src: icon, role: "presentation" }),
    dom.div({ className: "category-name" }, name));
  }
});
