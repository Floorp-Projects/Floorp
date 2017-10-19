/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div } = DOM;

// Components
const StackTrace = createFactory(require("devtools/client/shared/components/StackTrace"));

/**
 * This component represents a side panel responsible for
 * rendering stack-trace info for selected request.
 */
function StackTracePanel({
  connector,
  openLink,
  request,
  sourceMapService,
}) {
  let { stacktrace } = request.cause;

  return (
    div({ className: "panel-container" },
      StackTrace({
        stacktrace,
        onViewSourceInDebugger: ({ url, line }) => {
          return connector.viewSourceInDebugger(url, line);
        },
        sourceMapService,
        openLink,
      }),
    )
  );
}

StackTracePanel.displayName = "StackTracePanel";

StackTracePanel.propTypes = {
  connector: PropTypes.object.isRequired,
  request: PropTypes.object.isRequired,
  sourceMapService: PropTypes.object,
  openLink: PropTypes.func,
};

module.exports = StackTracePanel;
