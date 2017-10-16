/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { viewSourceInDebugger } = require("../connector/index");

const { div } = DOM;

// Components
const StackTrace = createFactory(require("devtools/client/shared/components/StackTrace"));

function StackTracePanel({
  openLink,
  request,
  sourceMapService,
}) {
  let { stacktrace } = request.cause;

  return (
    div({ className: "panel-container" },
      StackTrace({
        stacktrace,
        onViewSourceInDebugger: ({ url, line }) => viewSourceInDebugger(url, line),
        sourceMapService,
        openLink,
      }),
    )
  );
}

StackTracePanel.displayName = "StackTracePanel";

StackTracePanel.propTypes = {
  request: PropTypes.object.isRequired,
  // Service to enable the source map feature.
  sourceMapService: PropTypes.object,
  openLink: PropTypes.func,
};

module.exports = StackTracePanel;
