/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { getSourceNames, parseURL,
        isScratchpadScheme, getSourceMappedFile } = require("devtools/client/shared/source-utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const l10n = new LocalizationHelper("devtools/locale/components.properties");
const webl10n = new LocalizationHelper("devtools/locale/webconsole.properties");

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
    // Option to display a function name even if it's anonymous.
    showAnonymousFunctionName: PropTypes.bool,
    // Option to display a host name after the source link.
    showHost: PropTypes.bool,
    // Option to display a host name if the filename is empty or just '/'
    showEmptyPathAsHost: PropTypes.bool,
    // Option to display a full source instead of just the filename.
    showFullSourceUrl: PropTypes.bool,
    // Service to enable the source map feature for console.
    sourceMapService: PropTypes.object,
  },

  getDefaultProps() {
    return {
      showFunctionName: false,
      showAnonymousFunctionName: false,
      showHost: false,
      showEmptyPathAsHost: false,
      showFullSourceUrl: false,
    };
  },

  componentWillMount() {
    const sourceMapService = this.props.sourceMapService;
    if (sourceMapService) {
      const source = this.getSource();
      sourceMapService.subscribe(source, this.onSourceUpdated);
    }
  },

  componentWillUnmount() {
    const sourceMapService = this.props.sourceMapService;
    if (sourceMapService) {
      const source = this.getSource();
      sourceMapService.unsubscribe(source, this.onSourceUpdated);
    }
  },

  /**
   * Component method to update the FrameView when a resolved location is available
   * @param event
   * @param location
   */
  onSourceUpdated(event, location, resolvedLocation) {
    const frame = this.getFrame(resolvedLocation);
    this.setState({
      frame,
      isSourceMapped: true,
    });
  },

  /**
   * Utility method to convert the Frame object to the
   * Source Object model required by SourceMapService
   * @param frame
   * @returns {{url: *, line: *, column: *}}
   */
  getSource(frame) {
    frame = frame || this.props.frame;
    const { source, line, column } = frame;
    return {
      url: source,
      line,
      column,
    };
  },

  /**
   * Utility method to convert the Source object model to the
   * Frame object model required by FrameView class.
   * @param source
   * @returns {{source: *, line: *, column: *, functionDisplayName: *}}
   */
  getFrame(source) {
    const { url, line, column } = source;
    return {
      source: url,
      line,
      column,
      functionDisplayName: this.props.frame.functionDisplayName,
    };
  },

  render() {
    let frame, isSourceMapped;
    let {
      onClick,
      showFunctionName,
      showAnonymousFunctionName,
      showHost,
      showEmptyPathAsHost,
      showFullSourceUrl
    } = this.props;

    if (this.state && this.state.isSourceMapped) {
      frame = this.state.frame;
      isSourceMapped = this.state.isSourceMapped;
    } else {
      frame = this.props.frame;
    }

    let source = frame.source ? String(frame.source) : "";
    let line = frame.line != void 0 ? Number(frame.line) : null;
    let column = frame.column != void 0 ? Number(frame.column) : null;

    const { short, long, host } = getSourceNames(source);
    // Reparse the URL to determine if we should link this; `getSourceNames`
    // has already cached this indirectly. We don't want to attempt to
    // link to "self-hosted" and "(unknown)". However, we do want to link
    // to Scratchpad URIs.
    // Source mapped sources might not necessary linkable, but they
    // are still valid in the debugger.
    const isLinkable = !!(isScratchpadScheme(source) || parseURL(source))
      || isSourceMapped;
    const elements = [];
    const sourceElements = [];
    let sourceEl;

    let tooltip = long;

    // If the source is linkable and line > 0
    const shouldDisplayLine = isLinkable && line;

    // Exclude all falsy values, including `0`, as even
    // a number 0 for line doesn't make sense, and should not be displayed.
    // If source isn't linkable, don't attempt to append line and column
    // info, as this probably doesn't make sense.
    if (shouldDisplayLine) {
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

    if (showFunctionName) {
      let functionDisplayName = frame.functionDisplayName;
      if (!functionDisplayName && showAnonymousFunctionName) {
        functionDisplayName = webl10n.getStr("stacktrace.anonymousFunction");
      }

      if (functionDisplayName) {
        elements.push(
          dom.span({ className: "frame-link-function-display-name" },
            functionDisplayName)
        );
      }
    }

    let displaySource = showFullSourceUrl ? long : short;
    if (isSourceMapped) {
      displaySource = getSourceMappedFile(displaySource);
    } else if (showEmptyPathAsHost && (displaySource === "" || displaySource === "/")) {
      displaySource = host;
    }

    sourceElements.push(dom.span({
      className: "frame-link-filename",
    }, displaySource));

    // If source is linkable, and we have a line number > 0
    if (shouldDisplayLine) {
      let lineInfo = `:${line}`;
      // Add `data-line` attribute for testing
      attributes["data-line"] = line;

      // Intentionally exclude 0
      if (column) {
        lineInfo += `:${column}`;
        // Add `data-column` attribute for testing
        attributes["data-column"] = column;
      }

      sourceElements.push(dom.span({ className: "frame-link-line" }, lineInfo));
    }

    // Inner el is useful for achieving ellipsis on the left and correct LTR/RTL
    // ordering. See CSS styles for frame-link-source-[inner] and bug 1290056.
    let sourceInnerEl = dom.span({
      className: "frame-link-source-inner",
      title: isLinkable ?
        l10n.getFormatStr("frame.viewsourceindebugger", tooltip) : tooltip,
    }, sourceElements);

    // If source is not a URL (self-hosted, eval, etc.), don't make
    // it an anchor link, as we can't link to it.
    if (isLinkable) {
      sourceEl = dom.a({
        onClick: e => {
          e.preventDefault();
          onClick(this.getSource(frame));
        },
        href: source,
        className: "frame-link-source",
        draggable: false,
      }, sourceInnerEl);
    } else {
      sourceEl = dom.span({
        className: "frame-link-source",
      }, sourceInnerEl);
    }
    elements.push(sourceEl);

    if (showHost && host) {
      elements.push(dom.span({ className: "frame-link-host" }, host));
    }

    return dom.span(attributes, ...elements);
  }
});
