/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div } = dom;

/*
 * Response preview component
 * Display HTML content within a sandbox enabled iframe
 */
class HTMLPreview extends Component {
  static get propTypes() {
    return {
      responseContent: PropTypes.object.isRequired,
    };
  }

  componentDidMount() {
    const { container } = this.refs;
    const browser = container.ownerDocument.createElement("iframe");
    browser.setAttribute("sandbox", "");

    const { responseContent } = this.props;
    const htmlBody = responseContent ? responseContent.content.text : "";
    browser.setAttribute("srcdoc", htmlBody);

    container.appendChild(browser);
  }

  render() {
    return div({ className: "html-preview", ref: "container" });
  }
}

module.exports = HTMLPreview;
