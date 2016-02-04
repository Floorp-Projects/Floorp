/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React */

"use strict";

loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");

exports.TabHeaderComponent = React.createClass({
  displayName: "TabHeaderComponent",

  render() {
    let { name, id } = this.props;

    return React.createElement(
      "div", { className: "header" }, React.createElement(
        "h1", { id, className: "header-name" }, name));
  },
});
