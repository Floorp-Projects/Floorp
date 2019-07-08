/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div, iframe } = dom;

/*
 * Response preview component
 * Display HTML content within a sandbox enabled iframe
 */
function HTMLPreview({ responseContent }) {
  const htmlBody = responseContent ? responseContent.content.text : "";

  return div(
    { className: "html-preview" },
    iframe({
      sandbox: "",
      srcDoc: typeof htmlBody === "string" ? htmlBody : "",
    })
  );
}

HTMLPreview.displayName = "HTMLPreview";

HTMLPreview.propTypes = {
  requestContent: PropTypes.object.isRequired,
};

module.exports = HTMLPreview;
