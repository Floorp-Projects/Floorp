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
const GripMessageBody = createFactory(require("devtools/client/webconsole/new-console-output/components/grip-message-body").GripMessageBody);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon").MessageIcon);

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  message: PropTypes.object.isRequired,
};

function EvaluationResult(props) {
  const { message } = props;
  const {category, severity} = message;
  const icon = MessageIcon({severity: severity});

  const classes = ["message", "cm-s-mozilla"];

  if (category) {
    classes.push(category);
  }

  if (severity) {
    classes.push(severity);
  }

  return dom.div({
    className: classes.join(" ")
  },
    // @TODO add timestamp
    // @TODO add indent if needed with console.group
    icon,
    dom.span(
      {className: "message-body-wrapper message-body devtools-monospace"},
      dom.span({},
        GripMessageBody({grip: message.parameters})
      )
    )
  );
}

module.exports.EvaluationResult = EvaluationResult;
