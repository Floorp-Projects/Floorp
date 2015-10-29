/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { URL } = require("sdk/url");

const Frame = module.exports = createClass({
  displayName: "frame-view",

  propTypes: {
    frame: PropTypes.object.isRequired,
    toolbox: PropTypes.object.isRequired,
  },

  render() {
    let { toolbox, frame } = this.props;

    let url = new URL(frame.source);
    let spec = url.toString();
    let func = frame.functionDisplayName || "";
    let tooltip = `${func} (${spec}:${frame.line}:${frame.column})`;
    let onClick = () => toolbox.viewSourceInDebugger(spec, frame.line);

    let fields = [
      dom.span({ className: "frame-link-function-display-name" }, func),
      dom.a({ className: "frame-link-filename", onClick }, url.fileName),
      dom.span({ className: "frame-link-colon" }, ":"),
      dom.span({ className: "frame-link-line" }, frame.line),
      dom.span({ className: "frame-link-colon" }, ":"),
      dom.span({ className: "frame-link-column" }, frame.column)
    ];

    if (url.scheme === "http" || url.scheme === "https" || url.scheme === "ftp") {
      fields.push(dom.span({ className: "frame-link-host" }, url.host));
    }

    return dom.span({ className: "frame-link", title: tooltip }, ...fields);
  }
});
