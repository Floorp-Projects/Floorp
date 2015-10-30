/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N, parseSource } = require("../utils");

const Frame = module.exports = createClass({
  displayName: "frame-view",

  propTypes: {
    frame: PropTypes.object.isRequired,
    toolbox: PropTypes.object.isRequired,
  },

  render() {
    let { toolbox, frame } = this.props;
    const { short, long, host } = parseSource(frame);

    let func = frame.functionDisplayName || "";
    let tooltip = `${func} (${long}:${frame.line}:${frame.column})`;
    let viewTooltip = L10N.getFormatStr("viewsourceindebugger", `${long}:${frame.line}:${frame.column}`);
    let onClick = () => toolbox.viewSourceInDebugger(long, frame.line);

    let fields = [
      dom.span({ className: "frame-link-function-display-name" }, func),
      dom.a({ className: "frame-link-filename", onClick, title: viewTooltip }, short),
      dom.span({ className: "frame-link-colon" }, ":"),
      dom.span({ className: "frame-link-line" }, frame.line),
      dom.span({ className: "frame-link-colon" }, ":"),
      dom.span({ className: "frame-link-column" }, frame.column)
    ];

    if (host) {
      fields.push(dom.span({ className: "frame-link-host" }, host));
    }

    return dom.span({ className: "frame-link", title: tooltip }, ...fields);
  }
});
