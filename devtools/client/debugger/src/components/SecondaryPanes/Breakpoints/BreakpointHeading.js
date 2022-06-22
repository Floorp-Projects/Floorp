/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import PropTypes from "prop-types";

import { connect } from "../../../utils/connect";
import actions from "../../../actions";

import {
  getTruncatedFileName,
  getDisplayPath,
  getSourceQueryString,
  getFileURL,
} from "../../../utils/source";
import {
  getBreakpointsForSource,
  getContext,
  getThread,
} from "../../../selectors";

import SourceIcon from "../../shared/SourceIcon";

import showContextMenu from "./BreakpointHeadingsContextMenu";

class BreakpointHeading extends PureComponent {
  static get propTypes() {
    return {
      cx: PropTypes.object.isRequired,
      sources: PropTypes.array.isRequired,
      source: PropTypes.object.isRequired,
      selectSource: PropTypes.func.isRequired,
      thread: PropTypes.object.isRequired,
    };
  }
  onContextMenu = e => {
    showContextMenu({ ...this.props, contextMenuEvent: e });
  };

  render() {
    const { cx, sources, source, selectSource, thread } = this.props;

    const path = getDisplayPath(source, sources);
    const query = getSourceQueryString(source);

    return (
      <div
        className="breakpoint-heading"
        title={`${thread?.name} - ${getFileURL(source, false)}`}
        onClick={() => selectSource(cx, source.id)}
        onContextMenu={this.onContextMenu}
      >
        <SourceIcon
          source={source}
          modifier={icon =>
            ["file", "javascript"].includes(icon) ? null : icon
          }
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
  breakpointsForSource: getBreakpointsForSource(state, source.id),
  thread: getThread(state, source.thread),
});

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  enableBreakpointsInSource: actions.enableBreakpointsInSource,
  disableBreakpointsInSource: actions.disableBreakpointsInSource,
  removeBreakpointsInSource: actions.removeBreakpointsInSource,
})(BreakpointHeading);
