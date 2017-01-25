/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");

const { div, iframe } = DOM;

/*
 * Preview panel component
 * Display HTML content within a sandbox enabled iframe
 */
function PreviewPanel({
  request,
}) {
  const htmlBody = request.responseContent ?
    request.responseContent.content.text : "";

  return (
    div({ className: "panel-container" },
      iframe({
        sandbox: "",
        srcDoc: typeof htmlBody === "string" ? htmlBody : "",
      })
    )
  );
}

PreviewPanel.displayName = "PreviewPanel";

PreviewPanel.propTypes = {
  request: PropTypes.object.isRequired,
};

module.exports = PreviewPanel;
