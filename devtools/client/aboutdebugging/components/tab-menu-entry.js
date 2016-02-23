/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");

exports.TabMenuEntry = React.createClass({
  displayName: "TabMenuEntry",

  render() {
    let { icon, name, selected } = this.props;

    // Here .category, .category-icon, .category-name classnames are used to
    // apply common styles defined.
    let className = "category" + (selected ? " selected" : "");
    return React.createElement(
      "div", { className, onClick: this.onClick,
        "aria-selected": selected, role: "tab" },
        React.createElement("img", { className: "category-icon", src: icon,
          role: "presentation" }),
        React.createElement("div", { className: "category-name" }, name)
      );
  },

  onClick() {
    this.props.selectTab(this.props.tabId);
  }
});
