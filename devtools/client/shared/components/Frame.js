/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  getUnicodeUrl,
  getUnicodeUrlPath,
  getUnicodeHostname,
} = require("devtools/client/shared/unicode-url");
const {
  getSourceNames,
  parseURL,
  getSourceMappedFile,
} = require("devtools/client/shared/source-utils");
const { LocalizationHelper } = require("devtools/shared/l10n");
const { MESSAGE_SOURCE } = require("devtools/client/webconsole/constants");

const l10n = new LocalizationHelper(
  "devtools/client/locales/components.properties"
);
const webl10n = new LocalizationHelper(
  "devtools/client/locales/webconsole.properties"
);

function savedFrameToLocation(frame) {
  const { source: url, line, column, sourceId } = frame;
  return {
    url,
    line,
    column,
    // The sourceId will be a string if it's a source actor ID, otherwise
    // it is either a Spidermonkey-internal ID from a SavedFrame or missing,
    // and in either case we can't use the ID for anything useful.
    id: typeof sourceId === "string" ? sourceId : null,
  };
}

class Frame extends Component {
  static get propTypes() {
    return {
      // SavedFrame, or an object containing all the required properties.
      frame: PropTypes.shape({
        functionDisplayName: PropTypes.string,
        // This could be a SavedFrame with a numeric sourceId, or it could
        // be a SavedFrame-like client-side object, in which case the
        // "sourceId" will be a source actor ID.
        sourceId: PropTypes.oneOfType([PropTypes.string, PropTypes.number]),
        source: PropTypes.string.isRequired,
        line: PropTypes.oneOfType([PropTypes.string, PropTypes.number]),
        column: PropTypes.oneOfType([PropTypes.string, PropTypes.number]),
      }).isRequired,
      // Clicking on the frame link -- probably should link to the debugger.
      onClick: PropTypes.func,
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
      sourceMapURLService: PropTypes.object,
      // The source of the message
      messageSource: PropTypes.string,
    };
  }

  static get defaultProps() {
    return {
      showFunctionName: false,
      showAnonymousFunctionName: false,
      showHost: false,
      showEmptyPathAsHost: false,
      showFullSourceUrl: false,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      originalLocation: null,
    };
    this._locationChanged = this._locationChanged.bind(this);
  }

  componentWillMount() {
    if (this.props.sourceMapURLService) {
      const location = savedFrameToLocation(this.props.frame);
      // Many things that make use of this component either:
      // a) Pass in no sourceId because they have no way to know.
      // b) Pass in no sourceId because the actor wasn't created when the
      //    server sent its response.
      //
      // and due to that, we need to use subscribeByLocation in order to
      // handle both cases with an without an ID.
      this.unsubscribeSourceMapService = this.props.sourceMapURLService.subscribeByLocation(
        location,
        this._locationChanged
      );
    }
  }

  componentWillUnmount() {
    if (this.unsubscribeSourceMapService) {
      this.unsubscribeSourceMapService();
    }
  }

  _locationChanged(originalLocation) {
    this.setState({ originalLocation });
  }

  // eslint-disable-next-line complexity
  render() {
    const {
      frame,
      onClick,
      showFunctionName,
      showAnonymousFunctionName,
      showHost,
      showEmptyPathAsHost,
      showFullSourceUrl,
      messageSource,
    } = this.props;
    const { originalLocation } = this.state;

    const generatedLocation = savedFrameToLocation(frame);
    const currentLocation = originalLocation || generatedLocation;

    const source = currentLocation.url || "";
    const line =
      currentLocation.line != void 0 ? Number(currentLocation.line) : null;
    const column =
      currentLocation.column != void 0 ? Number(currentLocation.column) : null;

    const { short, long, host } = getSourceNames(source);
    const unicodeShort = getUnicodeUrlPath(short);
    const unicodeLong = getUnicodeUrl(long);
    const unicodeHost = host ? getUnicodeHostname(host) : "";

    // Reparse the URL to determine if we should link this; `getSourceNames`
    // has already cached this indirectly. We don't want to attempt to
    // link to "self-hosted" and "(unknown)".
    // Source mapped sources might not necessary linkable, but they
    // are still valid in the debugger.
    // If we have a source ID then we can show the source in the debugger.
    const isLinkable =
      originalLocation ||
      generatedLocation.id ||
      !!parseURL(generatedLocation.url);

    const elements = [];
    const sourceElements = [];
    let sourceEl;
    let tooltip = unicodeLong;

    // Exclude all falsy values, including `0`, as line numbers start with 1.
    if (line) {
      tooltip += `:${line}`;
      // Intentionally exclude 0
      if (column) {
        tooltip += `:${column}`;
      }
    }

    const attributes = {
      "data-url": long,
      className: "frame-link",
    };

    if (showFunctionName) {
      const functionDisplayName = frame.functionDisplayName;
      if (functionDisplayName || showAnonymousFunctionName) {
        elements.push(
          dom.span(
            {
              key: "function-display-name",
              className: "frame-link-function-display-name",
            },
            functionDisplayName ||
              webl10n.getStr("stacktrace.anonymousFunction")
          ),
          " "
        );
      }
    }

    let displaySource = showFullSourceUrl ? unicodeLong : unicodeShort;
    if (originalLocation) {
      displaySource = getSourceMappedFile(displaySource);
    } else if (
      showEmptyPathAsHost &&
      (displaySource === "" || displaySource === "/")
    ) {
      displaySource = host;
    }

    sourceElements.push(
      dom.span(
        {
          key: "filename",
          className: "frame-link-filename",
        },
        displaySource
      )
    );

    // If we have a line number > 0.
    if (line) {
      let lineInfo = `:${line}`;
      // Add `data-line` attribute for testing
      attributes["data-line"] = line;

      // Intentionally exclude 0
      if (column) {
        lineInfo += `:${column}`;
        // Add `data-column` attribute for testing
        attributes["data-column"] = column;
      }

      sourceElements.push(
        dom.span(
          {
            key: "line",
            className: "frame-link-line",
          },
          lineInfo
        )
      );
    }

    // Inner el is useful for achieving ellipsis on the left and correct LTR/RTL
    // ordering. See CSS styles for frame-link-source-[inner] and bug 1290056.
    let tooltipMessage;
    if (messageSource && messageSource === MESSAGE_SOURCE.CSS) {
      tooltipMessage = l10n.getFormatStr(
        "frame.viewsourceinstyleeditor",
        tooltip
      );
    } else {
      tooltipMessage = l10n.getFormatStr("frame.viewsourceindebugger", tooltip);
    }

    const sourceInnerEl = dom.span(
      {
        key: "source-inner",
        className: "frame-link-source-inner",
        title: isLinkable ? tooltipMessage : tooltip,
      },
      sourceElements
    );

    // If source is not a URL (self-hosted, eval, etc.), don't make
    // it an anchor link, as we can't link to it.
    if (isLinkable) {
      sourceEl = dom.a(
        {
          onClick: e => {
            e.preventDefault();
            e.stopPropagation();

            onClick(generatedLocation);
          },
          href: source,
          className: "frame-link-source",
          draggable: false,
        },
        sourceInnerEl
      );
    } else {
      sourceEl = dom.span(
        {
          key: "source",
          className: "frame-link-source",
        },
        sourceInnerEl
      );
    }
    elements.push(sourceEl);

    if (showHost && unicodeHost) {
      elements.push(" ");
      elements.push(
        dom.span(
          {
            key: "host",
            className: "frame-link-host",
          },
          unicodeHost
        )
      );
    }

    return dom.span(attributes, ...elements);
  }
}

module.exports = Frame;
