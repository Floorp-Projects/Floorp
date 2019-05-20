/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "../../../utils/connect";
import actions from "../../../actions";
import {
  getTruncatedFileName,
  getDisplayPath,
  getSourceQueryString,
  getFileURL,
} from "../../../utils/source";
import {
  getHasSiblingOfSameName,
  getBreakpointsForSource,
  getContext,
} from "../../../selectors";

import SourceIcon from "../../shared/SourceIcon";

import type { Source, Breakpoint, Context } from "../../../types";
import showContextMenu from "./BreakpointHeadingsContextMenu";

type Props = {
  cx: Context,
  sources: Source[],
  source: Source,
  hasSiblingOfSameName: boolean,
  breakpointsForSource: Breakpoint[],
  disableBreakpointsInSource: typeof actions.disableBreakpointsInSource,
  enableBreakpointsInSource: typeof actions.enableBreakpointsInSource,
  removeBreakpointsInSource: typeof actions.removeBreakpointsInSource,
  selectSource: typeof actions.selectSource,
};

class BreakpointHeading extends PureComponent<Props> {
  onContextMenu = e => {
    showContextMenu({ ...this.props, contextMenuEvent: e });
  };

  render() {
    const {
      cx,
      sources,
      source,
      hasSiblingOfSameName,
      selectSource,
    } = this.props;

    const path = getDisplayPath(source, sources);
    const query = hasSiblingOfSameName ? getSourceQueryString(source) : "";

    return (
      <div
        className="breakpoint-heading"
        title={getFileURL(source, false)}
        onClick={() => selectSource(cx, source.id)}
        onContextMenu={this.onContextMenu}
      >
        <SourceIcon
          source={source}
          shouldHide={icon => ["file", "javascript"].includes(icon)}
        />
        <div className="filename">
          {getTruncatedFileName(source, query)}
          {path && <span>{`../${path}/..`}</span>}
        </div>
      </div>
    );
  }
}

const mapStateToProps = (state, { source }) => ({
  cx: getContext(state),
  hasSiblingOfSameName: getHasSiblingOfSameName(state, source),
  breakpointsForSource: getBreakpointsForSource(state, source.id),
});

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    enableBreakpointsInSource: actions.enableBreakpointsInSource,
    disableBreakpointsInSource: actions.disableBreakpointsInSource,
    removeBreakpointsInSource: actions.removeBreakpointsInSource,
  }
)(BreakpointHeading);
