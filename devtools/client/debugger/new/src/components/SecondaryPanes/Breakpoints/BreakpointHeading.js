/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import { connect } from "react-redux";
import actions from "../../../actions";
import {
  getTruncatedFileName,
  getDisplayPath,
  getSourceQueryString,
  getFileURL
} from "../../../utils/source";
import { getHasSiblingOfSameName } from "../../../selectors";

import SourceIcon from "../../shared/SourceIcon";

import type { Source } from "../../../types";

type Props = {
  sources: Source[],
  source: Source,
  hasSiblingOfSameName: boolean,
  selectSource: typeof actions.selectSource
};

class BreakpointHeading extends PureComponent<Props> {
  render() {
    const { sources, source, hasSiblingOfSameName, selectSource } = this.props;

    const path = getDisplayPath(source, sources);
    const query = hasSiblingOfSameName ? getSourceQueryString(source) : "";

    return (
      <div
        className="breakpoint-heading"
        title={getFileURL(source, false)}
        onClick={() => selectSource(source.id)}
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
  hasSiblingOfSameName: getHasSiblingOfSameName(state, source)
});

export default connect(
  mapStateToProps,
  { selectSource: actions.selectSource }
)(BreakpointHeading);
