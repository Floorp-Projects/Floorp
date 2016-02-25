/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");

exports.TabHeader = React.createClass({
  displayName: "TabHeader",

  render() {
    let { name, id } = this.props;

    return React.createElement(
      "div", { className: "header" }, React.createElement(
        "h1", { id, className: "header-name" }, name));
  },
});
