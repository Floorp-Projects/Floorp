/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { getSelectedRequest } = require("../../selectors/index");

const { iframe } = DOM;

/*
 * Preview panel component
 * Display HTML content within a sandbox enabled iframe
 */
function PreviewPanel({
  srcDoc = "",
}) {
  return iframe({
    id: "response-preview",
    sandbox: "",
    srcDoc,
  });
}

PreviewPanel.displayName = "PreviewPanel";

PreviewPanel.propTypes = {
  srcDoc: PropTypes.string,
};

module.exports = connect(
  (state) => {
    const selectedRequest = getSelectedRequest(state);
    const htmlBody = selectedRequest && selectedRequest.responseContent ?
      selectedRequest.responseContent.content.text : "";
    const srcDoc = typeof htmlBody === "string" ? htmlBody : "";

    return {
      srcDoc,
    };
  }
)(PreviewPanel);
