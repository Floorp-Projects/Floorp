/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSourceNames, parseURL, isScratchpadScheme } = require("devtools/client/shared/source-utils");
const { LocalizationHelper } = require("devtools/client/shared/l10n");

const l10n = new LocalizationHelper("chrome://devtools/locale/components.properties");

module.exports = createClass({
  displayName: "Frame",

  propTypes: {
    // SavedFrame, or an object containing all the required properties.
    frame: PropTypes.shape({
      functionDisplayName: PropTypes.string,
      source: PropTypes.string.isRequired,
      line: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      column: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
    }).isRequired,
    // Clicking on the frame link -- probably should link to the debugger.
    onClick: PropTypes.func.isRequired,
    // Option to display a function name before the source link.
    showFunctionName: PropTypes.bool,
    // Option to display a host name after the source link.
    showHost: PropTypes.bool,
  },

  getDefaultProps() {
    return {
      showFunctionName: false,
      showHost: false,
    };
  },

  render() {
    let { onClick, frame, showFunctionName, showHost } = this.props;
    let source = frame.source ? String(frame.source) : "";
    let line = frame.line != void 0 ? Number(frame.line) : null;
    let column = frame.column != void 0 ? Number(frame.column) : null;

    const { short, long, host } = getSourceNames(source);
    // Reparse the URL to determine if we should link this; `getSourceNames`
    // has already cached this indirectly. We don't want to attempt to
    // link to "self-hosted" and "(unknown)". However, we do want to link
    // to Scratchpad URIs.
    const isLinkable = !!(isScratchpadScheme(source) || parseURL(source));
    const elements = [];
    const sourceElements = [];
    let sourceEl;

    let tooltip = long;
    // Exclude all falsy values, including `0`, as even
    // a number 0 for line doesn't make sense, and should not be displayed.
    // If source isn't linkable, don't attempt to append line and column
    // info, as this probably doesn't make sense.
    if (isLinkable && line) {
      tooltip += `:${line}`;
      // Intentionally exclude 0
      if (column) {
        tooltip += `:${column}`;
      }
    }

    let attributes = {
      "data-url": long,
      className: "frame-link",
    };

    if (showFunctionName && frame.functionDisplayName) {
      elements.push(
        dom.span({ className: "frame-link-function-display-name" },
                 frame.functionDisplayName)
      );
    }

    sourceElements.push(dom.span({
      className: "frame-link-filename",
    }, short));

    // If source is linkable, and we have a line number > 0
    if (isLinkable && line) {
      sourceElements.push(dom.span({ className: "frame-link-colon" }, ":"));
      sourceElements.push(dom.span({ className: "frame-link-line" }, line));
      // Intentionally exclude 0
      if (column) {
        sourceElements.push(dom.span({ className: "frame-link-colon" }, ":"));
        sourceElements.push(
          dom.span({ className: "frame-link-column" }, column)
        );
        // Add `data-column` attribute for testing
        attributes["data-column"] = column;
      }

      // Add `data-line` attribute for testing
      attributes["data-line"] = line;
    }

    // If source is not a URL (self-hosted, eval, etc.), don't make
    // it an anchor link, as we can't link to it.
    if (isLinkable) {
      sourceEl = dom.a({
        onClick: e => {
          e.preventDefault();
          onClick(e);
        },
        href: source,
        className: "frame-link-source",
        title: l10n.getFormatStr("frame.viewsourceindebugger", tooltip)
      }, sourceElements);
    } else {
      sourceEl = dom.span({
        className: "frame-link-source",
        title: tooltip,
      }, sourceElements);
    }
    elements.push(sourceEl);

    if (showHost && host) {
      elements.push(dom.span({ className: "frame-link-host" }, host));
    }

    return dom.span(attributes, ...elements);
  }
});
