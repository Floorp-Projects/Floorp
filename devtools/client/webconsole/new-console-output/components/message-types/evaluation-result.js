/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createElement,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { MessageRepeat } = require("devtools/client/webconsole/new-console-output/components/message-repeat");

EvaluationResult.displayName = "EvaluationResult";

EvaluationResult.propTypes = {
  message: PropTypes.object.isRequired,
};

function EvaluationResult(props) {
  const { message } = props;
  let PreviewComponent = getPreviewComponent(message.data);
  return createElement(PreviewComponent, { data: message.data });
}

function getPreviewComponent(data) {
  if (typeof data.class != "undefined") {
    switch (data.class) {
      case "Date":
        return require("devtools/client/webconsole/new-console-output/components/message-types/date-preview").DatePreview;
    }
  }
  return require("devtools/client/webconsole/new-console-output/components/message-types/default-renderer").DefaultRenderer;
}

module.exports.EvaluationResult = EvaluationResult;
