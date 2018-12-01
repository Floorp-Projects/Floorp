/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import classnames from "classnames";
import { getDocument } from "../../utils/editor";
import Svg from "../shared/Svg";

// eslint-disable-next-line max-len
import type { ColumnBreakpoint as ColumnBreakpointType } from "../../selectors/visibleColumnBreakpoints";

type Bookmark = {
  clear: Function
};

type Props = {
  callSite: Object,
  editor: Object,
  source: Object,
  enabled: boolean,
  toggleBreakpoint: (number, number) => void,
  columnBreakpoint: ColumnBreakpointType
};

const breakpointImg = document.createElement("div");
ReactDOM.render(<Svg name={"column-marker"} />, breakpointImg);
function makeBookmark(isActive, { onClick }) {
  const bp = breakpointImg.cloneNode(true);
  const className = isActive ? "active" : "disabled";
  bp.className = classnames("call-site", className);
  bp.onclick = onClick;
  return bp;
}

export default class CallSite extends PureComponent<Props> {
  addCallSite: Function;
  bookmark: ?Bookmark;

  addCallSite = (nextProps: ?Props) => {
    const { columnBreakpoint, source } = nextProps || this.props;
    const sourceId = source.id;
    const { line, column } = columnBreakpoint.location;
    const widget = makeBookmark(columnBreakpoint.enabled, {
      onClick: this.toggleBreakpoint
    });
    const doc = getDocument(sourceId);
    this.bookmark = doc.setBookmark({ line: line - 1, ch: column }, { widget });
  };

  clearCallSite = () => {
    if (this.bookmark) {
      this.bookmark.clear();
      this.bookmark = null;
    }
  };

  toggleBreakpoint = () => {
    const { columnBreakpoint, toggleBreakpoint } = this.props;
    const { line, column } = columnBreakpoint.location;
    toggleBreakpoint(line, column);
  };

  componentDidMount() {
    this.addCallSite();
  }

  componentWillUnmount() {
    this.clearCallSite();
  }

  componentDidUpdate() {
    this.clearCallSite();
    this.addCallSite();
  }

  render() {
    return null;
  }
}
