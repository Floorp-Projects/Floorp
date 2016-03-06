/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("./utils/l10n");
const { DOM: dom, createClass, PropTypes } =
  require("devtools/client/shared/vendor/react");

module.exports = createClass({

  displayName: "GlobalToolbar",

  propTypes: {
    onExit: PropTypes.func.isRequired,
  },

  render() {
    let {
      onExit,
    } = this.props;

    return dom.header(
      {
        id: "global-toolbar",
        className: "toolbar",
      },
      dom.span(
        {
          className: "title",
        },
        getStr("responsive.title")),
      dom.button({
        id: "global-exit-button",
        className: "toolbar-button",
        title: getStr("responsive.exit"),
        onClick: onExit,
      })
    );
  },

});
