/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component, memo } from "react";
import PropTypes from "prop-types";

import classNames from "classnames";

import AccessibleImage from "../../shared/AccessibleImage";
import { formatDisplayName } from "../../../utils/pause/frames";
import { getFilename, getFileURL } from "../../../utils/source";
import FrameMenu from "./FrameMenu";
import FrameIndent from "./FrameIndent";
import actions from "../../../actions";

import type { Frame, ThreadContext } from "../../../types";

type FrameTitleProps = {
  frame: Frame,
  options: Object,
  l10n: Object,
};

function FrameTitle({ frame, options = {}, l10n }: FrameTitleProps) {
  const displayName = formatDisplayName(frame, options, l10n);
  return <span className="title">{displayName}</span>;
}

type FrameLocationProps = { frame: Frame, displayFullUrl: boolean };

const FrameLocation = memo(
  ({ frame, displayFullUrl = false }: FrameLocationProps) => {
    if (!frame.source) {
      return null;
    }

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

    const { location, source } = frame;
    const filename = displayFullUrl
      ? getFileURL(source, false)
      : getFilename(source);

    return (
      <span className="location" title={source.url}>
        <span className="filename">{filename}</span>:
        <span className="line">{location.line}</span>
      </span>
    );
  }
);

FrameLocation.displayName = "FrameLocation";

type FrameComponentProps = {
  cx: ThreadContext,
  frame: Frame,
  selectedFrame: Frame,
  copyStackTrace: Function,
  toggleFrameworkGrouping: Function,
  selectFrame: typeof actions.selectFrame,
  selectLocation: typeof actions.selectLocation,
  frameworkGroupingOn: boolean,
  hideLocation: boolean,
  shouldMapDisplayName: boolean,
  toggleBlackBox: Function,
  displayFullUrl: boolean,
  getFrameTitle?: string => string,
  disableContextMenu: boolean,
  panel: "debugger" | "webconsole",
};

export default class FrameComponent extends Component<FrameComponentProps> {
  static defaultProps = {
    hideLocation: false,
    shouldMapDisplayName: true,
    disableContextMenu: false,
  };

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  get isDebugger() {
    return this.props.panel == "debugger";
  }

  onContextMenu(event: SyntheticMouseEvent<HTMLElement>) {
    const {
      frame,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn,
      cx,
    } = this.props;
    FrameMenu(
      frame,
      frameworkGroupingOn,
      { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox },
      event,
      cx
    );
  }

  onMouseDown(
    e: SyntheticMouseEvent<HTMLElement>,
    frame: Frame,
    selectedFrame: Frame
  ) {
    if (e.button !== 0) {
      return;
    }

    this.props.selectFrame(this.props.cx, frame);
  }

  onKeyUp(
    event: SyntheticKeyboardEvent<HTMLElement>,
    frame: Frame,
    selectedFrame: Frame
  ) {
    if (event.key != "Enter") {
      return;
    }

    this.props.selectFrame(this.props.cx, frame);
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
    } = this.props;
    const { l10n } = this.context;

    const className = classNames("frame", {
      selected: selectedFrame && selectedFrame.id === frame.id,
    });

    if (!frame.source) {
      throw new Error("no frame source");
    }

    const title = getFrameTitle
      ? getFrameTitle(
          `${getFileURL(frame.source, false)}:${frame.location.line}`
        )
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
          <FrameLocation frame={frame} displayFullUrl={displayFullUrl} />
        )}
        {this.isSelectable && <br className="clipboard-only" />}
      </div>
    );
  }
}

FrameComponent.displayName = "Frame";
FrameComponent.contextTypes = { l10n: PropTypes.object };
