/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component, memo } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import AccessibleImage from "../../shared/AccessibleImage";
import { formatDisplayName } from "../../../utils/pause/frames/index";
import { getFilename, getFileURL } from "../../../utils/source";
import FrameIndent from "./FrameIndent";
const classnames = require("resource://devtools/client/shared/classnames.js");

function FrameTitle({ frame, options = {}, l10n }) {
  const displayName = formatDisplayName(frame, options, l10n);
  return React.createElement(
    "span",
    {
      className: "title",
    },
    displayName
  );
}

FrameTitle.propTypes = {
  frame: PropTypes.object.isRequired,
  options: PropTypes.object.isRequired,
  l10n: PropTypes.object.isRequired,
  showFrameContextMenu: PropTypes.func.isRequired,
};

function getFrameLocation(frame, shouldDisplayOriginalLocation) {
  if (shouldDisplayOriginalLocation) {
    return frame.location;
  }
  return frame.generatedLocation || frame.location;
}
const FrameLocation = memo(
  ({ frame, displayFullUrl = false, shouldDisplayOriginalLocation }) => {
    if (frame.library) {
      return React.createElement(
        "span",
        {
          className: "location",
        },
        frame.library,
        React.createElement(AccessibleImage, {
          className: `annotation-logo ${frame.library.toLowerCase()}`,
        })
      );
    }
    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const filename = displayFullUrl
      ? getFileURL(location.source, false)
      : getFilename(location.source);
    return React.createElement(
      "span",
      {
        className: "location",
        title: location.source.url,
      },
      React.createElement(
        "span",
        {
          className: "filename",
        },
        filename
      ),
      ":",
      React.createElement(
        "span",
        {
          className: "line",
        },
        location.line
      )
    );
  }
);
FrameLocation.displayName = "FrameLocation";

FrameLocation.propTypes = {
  frame: PropTypes.object.isRequired,
  displayFullUrl: PropTypes.bool.isRequired,
};

export default class FrameComponent extends Component {
  static defaultProps = {
    hideLocation: false,
    shouldMapDisplayName: true,
    disableContextMenu: false,
  };

  static get propTypes() {
    return {
      disableContextMenu: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      frame: PropTypes.object.isRequired,
      getFrameTitle: PropTypes.func,
      hideLocation: PropTypes.bool.isRequired,
      isInGroup: PropTypes.bool,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectedFrame: PropTypes.object,
      shouldMapDisplayName: PropTypes.bool.isRequired,
      shouldDisplayOriginalLocation: PropTypes.bool.isRequired,
      showFrameContextMenu: PropTypes.func.isRequired,
    };
  }

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  get isDebugger() {
    return this.props.panel == "debugger";
  }

  onContextMenu(event) {
    event.stopPropagation();
    event.preventDefault();

    const { frame } = this.props;
    this.props.showFrameContextMenu(event, frame);
  }

  onMouseDown(e, frame, selectedFrame) {
    if (e.button !== 0) {
      return;
    }

    this.props.selectFrame(frame);
  }

  onKeyUp(event, frame, selectedFrame) {
    if (event.key != "Enter") {
      return;
    }

    this.props.selectFrame(frame);
  }

  render() {
    const {
      frame,
      selectedFrame,
      hideLocation,
      shouldMapDisplayName,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      shouldDisplayOriginalLocation,
      isInGroup,
    } = this.props;
    const { l10n } = this.context;

    const className = classnames("frame", {
      selected: selectedFrame && selectedFrame.id === frame.id,
    });

    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const title = getFrameTitle
      ? getFrameTitle(`${getFileURL(location.source, false)}:${location.line}`)
      : undefined;
    return React.createElement(
      "div",
      {
        role: "listitem",
        key: frame.id,
        className: className,
        onMouseDown: e => this.onMouseDown(e, frame, selectedFrame),
        onKeyUp: e => this.onKeyUp(e, frame, selectedFrame),
        onContextMenu: disableContextMenu ? null : e => this.onContextMenu(e),
        tabIndex: 0,
        title: title,
      },
      frame.asyncCause &&
        React.createElement(
          "span",
          {
            className: "location-async-cause",
          },
          this.isSelectable && React.createElement(FrameIndent, null),
          this.isDebugger
            ? React.createElement(
                "span",
                {
                  className: "async-label",
                },
                frame.asyncCause
              )
            : l10n.getFormatStr("stacktrace.asyncStack", frame.asyncCause),
          this.isSelectable &&
            React.createElement("br", {
              className: "clipboard-only",
            })
        ),
      this.isSelectable &&
        React.createElement(FrameIndent, {
          indentLevel: isInGroup ? 2 : 1,
        }),
      React.createElement(FrameTitle, {
        frame,
        options: {
          shouldMapDisplayName,
        },
        l10n,
      }),
      !hideLocation &&
        React.createElement(
          "span",
          {
            className: "clipboard-only",
          },
          " "
        ),
      !hideLocation &&
        React.createElement(FrameLocation, {
          frame,
          displayFullUrl,
          shouldDisplayOriginalLocation,
        }),
      this.isSelectable &&
        React.createElement("br", {
          className: "clipboard-only",
        })
    );
  }
}

FrameComponent.displayName = "Frame";
FrameComponent.contextTypes = { l10n: PropTypes.object };
