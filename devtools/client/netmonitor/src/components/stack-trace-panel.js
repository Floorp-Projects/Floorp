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
const StackTrace = createFactory(require("devtools/client/shared/components/stack-trace"));

function StackTracePanel({ request }) {
  let { stacktrace } = request.cause;

  return (
    div({ className: "panel-container" },
      StackTrace({
        stacktrace,
        onViewSourceInDebugger: ({ url, line }) => viewSourceInDebugger(url, line),
      }),
    )
  );
}

StackTracePanel.displayName = "StackTracePanel";

StackTracePanel.propTypes = {
  request: PropTypes.object.isRequired,
};

module.exports = StackTracePanel;
