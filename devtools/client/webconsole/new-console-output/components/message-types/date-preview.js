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

DatePreview.displayName = "DatePreview";

DatePreview.propTypes = {
  message: PropTypes.object.isRequired,
};

function DatePreview(props) {
  const { data } = props;
  const { preview } = data;

  const dateString = new Date(preview.timestamp).toISOString();
  const textNodes = [
    VariablesViewLink({
      objectActor: data,
      label: "Date"
    }),
    dom.span({ className: "cm-string-2" }, ` ${dateString}`)
  ];

  return dom.div({ className: "message cm-s-mozilla" },
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
