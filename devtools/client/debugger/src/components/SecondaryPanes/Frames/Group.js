/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import PropTypes from "prop-types";
import classNames from "classnames";

import { getLibraryFromUrl } from "../../../utils/pause/frames";

import FrameMenu from "./FrameMenu";
import AccessibleImage from "../../shared/AccessibleImage";
import FrameComponent from "./Frame";

import "./Group.css";

import actions from "../../../actions";
import type { Frame, ThreadContext } from "../../../types";
import Badge from "../../shared/Badge";
import FrameIndent from "./FrameIndent";

type FrameLocationProps = { frame: Frame, expanded: boolean };
function FrameLocation({ frame, expanded }: FrameLocationProps) {
  const library = frame.library || getLibraryFromUrl(frame);
  if (!library) {
    return null;
  }

  const arrowClassName = classNames("arrow", { expanded });
  return (
    <span className="group-description">
      <AccessibleImage className={arrowClassName} />
      <AccessibleImage className={`annotation-logo ${library.toLowerCase()}`} />
      <span className="group-description-name">{library}</span>
    </span>
  );
}

FrameLocation.displayName = "FrameLocation";

type Props = {
  cx: ThreadContext,
  group: Frame[],
  selectedFrame: Frame,
  selectFrame: typeof actions.selectFrame,
  toggleFrameworkGrouping: Function,
  copyStackTrace: Function,
  toggleBlackBox: Function,
  frameworkGroupingOn: boolean,
  displayFullUrl: boolean,
  getFrameTitle?: string => string,
  disableContextMenu: boolean,
  selectable: boolean,
};

type State = {
  expanded: boolean,
};

export default class Group extends Component<Props, State> {
  toggleFrames: Function;

  constructor(...args: any[]) {
    super(...args);
    this.state = { expanded: false };
  }

  onContextMenu(event: SyntheticMouseEvent<HTMLElement>) {
    const {
      group,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn,
    } = this.props;
    const frame = group[0];
    FrameMenu(
      frame,
      frameworkGroupingOn,
      { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox },
      event
    );
  }

  toggleFrames = (event: SyntheticMouseEvent<HTMLElement>) => {
    event.stopPropagation();
    this.setState(prevState => ({ expanded: !prevState.expanded }));
  };

  renderFrames() {
    const {
      cx,
      group,
      selectFrame,
      selectedFrame,
      toggleFrameworkGrouping,
      frameworkGroupingOn,
      toggleBlackBox,
      copyStackTrace,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      selectable,
    } = this.props;

    const { expanded } = this.state;
    if (!expanded) {
      return null;
    }

    return (
      <div className="frames-list">
        {group.reduce((acc, frame, i) => {
          if (selectable) {
            acc.push(<FrameIndent key={`frame-indent-${i}`} />);
          }
          return acc.concat(
            <FrameComponent
              cx={cx}
              copyStackTrace={copyStackTrace}
              frame={frame}
              frameworkGroupingOn={frameworkGroupingOn}
              hideLocation={true}
              key={frame.id}
              selectedFrame={selectedFrame}
              selectFrame={selectFrame}
              shouldMapDisplayName={false}
              toggleBlackBox={toggleBlackBox}
              toggleFrameworkGrouping={toggleFrameworkGrouping}
              displayFullUrl={displayFullUrl}
              getFrameTitle={getFrameTitle}
              disableContextMenu={disableContextMenu}
              selectable={selectable}
            />
          );
        }, [])}
      </div>
    );
  }

  renderDescription() {
    const { l10n } = this.context;
    const { selectable, group } = this.props;

    const frame = group[0];
    const expanded = this.state.expanded;
    const l10NEntry = this.state.expanded
      ? "callStack.group.collapseTooltip"
      : "callStack.group.expandTooltip";
    const title = l10n.getFormatStr(l10NEntry, frame.library);

    return (
      <div
        role="listitem"
        key={frame.id}
        className={classNames("group")}
        onClick={this.toggleFrames}
        tabIndex={0}
        title={title}
      >
        {selectable && <FrameIndent />}
        <FrameLocation frame={frame} expanded={expanded} />
        {selectable && <span className="clipboard-only"> </span>}
        <Badge>{this.props.group.length}</Badge>
        {selectable && <br className="clipboard-only" />}
      </div>
    );
  }

  render() {
    const { expanded } = this.state;
    const { disableContextMenu } = this.props;
    return (
      <div
        className={classNames("frames-group", { expanded })}
        onContextMenu={disableContextMenu ? null : e => this.onContextMenu(e)}
      >
        {this.renderDescription()}
        {this.renderFrames()}
      </div>
    );
  }
}

Group.displayName = "Group";
Group.contextTypes = { l10n: PropTypes.object };
