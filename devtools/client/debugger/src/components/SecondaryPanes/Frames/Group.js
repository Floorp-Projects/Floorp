/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import classNames from "classnames";

import { getLibraryFromUrl } from "../../../utils/pause/frames";

import FrameMenu from "./FrameMenu";
import AccessibleImage from "../../shared/AccessibleImage";
import FrameComponent from "./Frame";

import "./Group.css";

import Badge from "../../shared/Badge";
import FrameIndent from "./FrameIndent";

function FrameLocation({ frame, expanded }) {
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

export default class Group extends Component {
  constructor(...args) {
    super(...args);
    this.state = { expanded: false };
  }

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  onContextMenu(event) {
    const {
      group,
      copyStackTrace,
      toggleFrameworkGrouping,
      toggleBlackBox,
      frameworkGroupingOn,
      cx,
    } = this.props;
    const frame = group[0];
    FrameMenu(
      frame,
      frameworkGroupingOn,
      { copyStackTrace, toggleFrameworkGrouping, toggleBlackBox },
      event,
      cx
    );
  }

  toggleFrames = event => {
    event.stopPropagation();
    this.setState(prevState => ({ expanded: !prevState.expanded }));
  };

  renderFrames() {
    const {
      cx,
      group,
      selectFrame,
      selectLocation,
      selectedFrame,
      toggleFrameworkGrouping,
      frameworkGroupingOn,
      toggleBlackBox,
      copyStackTrace,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      restart,
    } = this.props;

    const { expanded } = this.state;
    if (!expanded) {
      return null;
    }

    return (
      <div className="frames-list">
        {group.reduce((acc, frame, i) => {
          if (this.isSelectable) {
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
              selectLocation={selectLocation}
              shouldMapDisplayName={false}
              toggleBlackBox={toggleBlackBox}
              toggleFrameworkGrouping={toggleFrameworkGrouping}
              displayFullUrl={displayFullUrl}
              getFrameTitle={getFrameTitle}
              disableContextMenu={disableContextMenu}
              panel={panel}
              restart={restart}
            />
          );
        }, [])}
      </div>
    );
  }

  renderDescription() {
    const { l10n } = this.context;
    const { group } = this.props;
    const { expanded } = this.state;

    const frame = group[0];
    const l10NEntry = expanded
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
        {this.isSelectable && <FrameIndent />}
        <FrameLocation frame={frame} expanded={expanded} />
        {this.isSelectable && <span className="clipboard-only"> </span>}
        <Badge>{this.props.group.length}</Badge>
        {this.isSelectable && <br className="clipboard-only" />}
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
