/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component, memo } from "react";
import PropTypes from "prop-types";

import AccessibleImage from "../../shared/AccessibleImage";
import { formatDisplayName } from "../../../utils/pause/frames";
import { getFilename, getFileURL } from "../../../utils/source";
import FrameIndent from "./FrameIndent";
const classnames = require("devtools/client/shared/classnames.js");

function FrameTitle({ frame, options = {}, l10n }) {
  const displayName = formatDisplayName(frame, options, l10n);
  return <span className="title">{displayName}</span>;
}

FrameTitle.propTypes = {
  frame: PropTypes.object.isRequired,
  options: PropTypes.object.isRequired,
  l10n: PropTypes.object.isRequired,
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
      return (
        <span className="location">
          {frame.library}
          <AccessibleImage
            className={`annotation-logo ${frame.library.toLowerCase()}`}
          />
        </span>
      );
    }

    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const filename = displayFullUrl
      ? getFileURL(location.source, false)
      : getFilename(location.source);

    return (
      <span className="location" title={location.source.url}>
        <span className="filename">{filename}</span>:
        <span className="line">{location.line}</span>
      </span>
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
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectedFrame: PropTypes.object,
      shouldMapDisplayName: PropTypes.bool.isRequired,
      shouldDisplayOriginalLocation: PropTypes.bool.isRequired,
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
    } = this.props;
    const { l10n } = this.context;

    const className = classnames("frame", {
      selected: selectedFrame && selectedFrame.id === frame.id,
    });

    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const title = getFrameTitle
      ? getFrameTitle(`${getFileURL(location.source, false)}:${location.line}`)
      : undefined;

    return (
      <div
        role="listitem"
        key={frame.id}
        className={className}
        onMouseDown={e => this.onMouseDown(e, frame, selectedFrame)}
        onKeyUp={e => this.onKeyUp(e, frame, selectedFrame)}
        onContextMenu={disableContextMenu ? null : e => this.onContextMenu(e)}
        tabIndex={0}
        title={title}
      >
        {frame.asyncCause && (
          <span className="location-async-cause">
            {this.isSelectable && <FrameIndent />}
            {this.isDebugger ? (
              <span className="async-label">{frame.asyncCause}</span>
            ) : (
              l10n.getFormatStr("stacktrace.asyncStack", frame.asyncCause)
            )}
            {this.isSelectable && <br className="clipboard-only" />}
          </span>
        )}
        {this.isSelectable && <FrameIndent />}
        <FrameTitle
          frame={frame}
          options={{ shouldMapDisplayName }}
          l10n={l10n}
        />
        {!hideLocation && <span className="clipboard-only"> </span>}
        {!hideLocation && (
          <FrameLocation
            frame={frame}
            displayFullUrl={displayFullUrl}
            shouldDisplayOriginalLocation={shouldDisplayOriginalLocation}
          />
        )}
        {this.isSelectable && <br className="clipboard-only" />}
      </div>
    );
  }
}

FrameComponent.displayName = "Frame";
FrameComponent.contextTypes = { l10n: PropTypes.object };
