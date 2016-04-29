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
  PropTypes
} = require("devtools/client/shared/vendor/react");

const VariablesViewLink = createFactory(require("devtools/client/webconsole/new-console-output/components/variables-view-link").VariablesViewLink);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

DatePreview.displayName = "DatePreview";

DatePreview.propTypes = {
  data: PropTypes.object.isRequired,
};

function DatePreview(props) {
  const { data, category, severity } = props;
  const { preview } = data;

  const dateString = new Date(preview.timestamp).toISOString();
  const textNodes = [
    VariablesViewLink({
      objectActor: data,
      label: "Date"
    }),
    dom.span({ className: "cm-string-2" }, ` ${dateString}`)
  ];
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
    dom.span({
      className: "message-body-wrapper message-body devtools-monospace"
    }, dom.span({},
        dom.span({ className: "class-Date" },
          textNodes
        )
      )
    )
  );
}

module.exports.DatePreview = DatePreview;
