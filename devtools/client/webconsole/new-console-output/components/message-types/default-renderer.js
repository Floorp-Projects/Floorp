/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createFactory,
  DOM: dom,
} = require("devtools/client/shared/vendor/react");
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

DefaultRenderer.displayName = "DefaultRenderer";

function DefaultRenderer(props) {
  const { category, severity } = props;

  const icon = MessageIcon({ severity });

  // @TODO Use of "is" is a temporary hack to get the category and severity
  // attributes to be applied. There are targeted in webconsole's CSS rules,
  // so if we remove this hack, we have to modify the CSS rules accordingly.
  return dom.div({
    class: "message cm-s-mozilla",
    is: "fdt-message",
    category: category,
    severity: severity
  },
    icon,
    "This evaluation result type is not supported yet."
  );
}

module.exports.DefaultRenderer = DefaultRenderer;
