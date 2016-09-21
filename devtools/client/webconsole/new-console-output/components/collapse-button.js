/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createClass,
  DOM: dom,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

const CollapseButton = createClass({

  displayName: "CollapseButton",

  propTypes: {
    open: PropTypes.bool.isRequired,
  },

  render: function () {
    const { open, onClick } = this.props;

    let classes = ["theme-twisty"];

    if (open) {
      classes.push("open");
    }

    return dom.a({
      className: classes.join(" "),
      onClick,
      title: l10n.getStr("messageToggleDetails"),
    });
  }
});

module.exports.CollapseButton = CollapseButton;
