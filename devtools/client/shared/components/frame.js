/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const STRINGS_URI = "chrome://devtools/locale/components.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSourceNames } = require("devtools/client/shared/source-utils");
const UNKNOWN_SOURCE_STRING = L10N.getStr("frame.unknownSource");

const Frame = module.exports = createClass({
  displayName: "Frame",

  getDefaultProps() {
    return {
      showFunctionName: false,
      showHost: false,
    };
  },

  propTypes: {
    // SavedFrame, or an object containing all the required properties.
    frame: PropTypes.shape({
      functionDisplayName: PropTypes.string,
      source: PropTypes.string.isRequired,
      line: PropTypes.number.isRequired,
      column: PropTypes.number.isRequired,
    }).isRequired,
    // Clicking on the frame link -- probably should link to the debugger.
    onClick: PropTypes.func.isRequired,
    // Option to display a function name before the source link.
    showFunctionName: PropTypes.bool,
    // Option to display a host name after the source link.
    showHost: PropTypes.bool,
  },

  render() {
    let { onClick, frame, showFunctionName, showHost } = this.props;

    const { short, long, host } = getSourceNames(frame.source, UNKNOWN_SOURCE_STRING);

    let tooltip = `${long}:${frame.line}`;
    if (frame.column) {
      tooltip += `:${frame.column}`;
    }

    let sourceString = `${frame.source}:${frame.line}`;
    if (frame.column) {
      sourceString += `:${frame.column}`;
    }

    let onClickTooltipString = L10N.getFormatStr("frame.viewsourceindebugger", sourceString);

    let fields = [
      dom.a({
        className: "frame-link-filename",
        onClick,
        title: onClickTooltipString
      }, short),
      dom.span({ className: "frame-link-colon" }, ":"),
      dom.span({ className: "frame-link-line" }, frame.line),
    ];

    if (frame.column != null) {
      fields.push(dom.span({ className: "frame-link-colon" }, ":"));
      fields.push(dom.span({ className: "frame-link-column" }, frame.column));
    }

    if (showFunctionName && frame.functionDisplayName) {
      fields.unshift(dom.span({ className: "frame-link-function-display-name" }, frame.functionDisplayName));
    }

    if (showHost && host) {
      fields.push(dom.span({ className: "frame-link-host" }, host));
    }

    return dom.span({ className: "frame-link", title: tooltip }, ...fields);
  }
});
