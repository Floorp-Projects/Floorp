/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSourceNames } = require("devtools/client/shared/source-utils");

const Frame = module.exports = createClass({
  displayName: "Frame",

  propTypes: {
    // SavedFrame
    frame: PropTypes.object.isRequired,
    // Clicking on the frame link -- probably should link to the debugger.
    onClick: PropTypes.func.isRequired,
    // Tooltip to display when hovering over the link to the frame;
    // Something like "View source in debugger -> http://foo.com/file.js:100:2".
    onClickTooltipString: PropTypes.string.isRequired,
    // Source to display when cannot determine a good display name.
    // Something like "(unknown)".
    unknownSourceString: PropTypes.string.isRequired,
  },

  render() {
    let { onClick, frame, onClickTooltipString, unknownSourceString } = this.props;
    const { short, long, host } = getSourceNames(frame.source, unknownSourceString);

    let func = frame.functionDisplayName || "";
    let tooltip = `${func} (${long}:${frame.line}:${frame.column})`;

    let fields = [
      dom.span({ className: "frame-link-function-display-name" }, func),
      dom.a({
        className: "frame-link-filename",
        onClick,
        title: onClickTooltipString
      }, short),
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
