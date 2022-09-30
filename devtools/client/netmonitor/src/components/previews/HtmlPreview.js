/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

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
    const iframe = container.ownerDocument.createXULElement("iframe");
    this.iframe = iframe;
    iframe.setAttribute("type", "content");
    iframe.setAttribute("remote", "true");
    iframe.addEventListener("mousedown", e => e.preventDefault(), {
      capture: true,
    });
    container.appendChild(iframe);

    // browsingContext attribute is only available after the iframe
    // is attached to the DOM Tree.
    iframe.browsingContext.allowJavascript = false;

    const { responseContent } = this.props;
    const htmlBody = responseContent ? responseContent.content.text : "";
    iframe.setAttribute("src", "data:text/html," + htmlBody);
  }

  componentDidUpdate() {
    const { responseContent } = this.props;
    const htmlBody = responseContent ? responseContent.content.text : "";
    this.iframe.setAttribute("src", "data:text/html," + htmlBody);
  }

  componentWillUnmount() {
    this.iframe.remove();
  }

  render() {
    return dom.div({ className: "html-preview", ref: "container" });
  }
}

module.exports = HTMLPreview;
