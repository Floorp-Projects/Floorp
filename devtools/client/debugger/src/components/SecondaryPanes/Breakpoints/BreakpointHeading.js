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
import { createLocation } from "../../../utils/location";
import {
  getBreakpointsForSource,
  getContext,
  getFirstSourceActorForGeneratedSource,
} from "../../../selectors";

import SourceIcon from "../../shared/SourceIcon";

import showContextMenu from "./BreakpointHeadingsContextMenu";

class BreakpointHeading extends PureComponent {
  static get propTypes() {
    return {
      cx: PropTypes.object.isRequired,
      sources: PropTypes.array.isRequired,
      source: PropTypes.object.isRequired,
      firstSourceActor: PropTypes.object,
      selectSource: PropTypes.func.isRequired,
    };
  }
  onContextMenu = e => {
    showContextMenu({ ...this.props, contextMenuEvent: e });
  };

  render() {
    const { cx, sources, source, selectSource } = this.props;

    const path = getDisplayPath(source, sources);
    const query = getSourceQueryString(source);

    return (
      <div
        className="breakpoint-heading"
        title={getFileURL(source, false)}
        onClick={() => selectSource(cx, source)}
        onContextMenu={this.onContextMenu}
      >
        <SourceIcon
          // Breakpoints are displayed per source and may relate to many source actors.
          // Arbitrarily pick the first source actor to compute the matching source icon
          // The source actor is used to pick one specific source text content and guess
          // the related framework icon.
          location={createLocation({
            source,
            sourceActor: this.props.firstSourceActor,
          })}
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
  firstSourceActor: getFirstSourceActorForGeneratedSource(state, source.id),
});

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  enableBreakpointsInSource: actions.enableBreakpointsInSource,
  disableBreakpointsInSource: actions.disableBreakpointsInSource,
  removeBreakpointsInSource: actions.removeBreakpointsInSource,
})(BreakpointHeading);
