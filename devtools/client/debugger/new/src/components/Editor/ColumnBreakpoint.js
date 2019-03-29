/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
import classnames from "classnames";
import { getDocument } from "../../utils/editor";
import Svg from "../shared/Svg";
import { showMenu } from "devtools-contextmenu";
import { breakpointItems, createBreakpointItems } from "./menus/breakpoints";

// eslint-disable-next-line max-len
import type { ColumnBreakpoint as ColumnBreakpointType } from "../../selectors/visibleColumnBreakpoints";
import type { BreakpointItemActions } from "./menus/breakpoints";
import type { Source } from "../../types";

type Bookmark = {
  clear: Function
};

type Props = {
  editor: Object,
  source: Source,
  columnBreakpoint: ColumnBreakpointType,
  breakpointActions: BreakpointItemActions
};

const breakpointImg = document.createElement("button");
ReactDOM.render(<Svg name={"column-marker"} />, breakpointImg);

function makeBookmark({ breakpoint }, { onClick, onContextMenu }) {
  const bp = breakpointImg.cloneNode(true);

  const isActive = breakpoint && !breakpoint.disabled;
  const isDisabled = breakpoint && breakpoint.disabled;
  const condition = breakpoint && breakpoint.options.condition;
  const logValue = breakpoint && breakpoint.options.logValue;

  bp.className = classnames("column-breakpoint", {
    "has-condition": condition,
    "has-log": logValue,
    active: isActive,
    disabled: isDisabled
  });

  if (condition) {
    bp.setAttribute("title", condition);
  }
  bp.onclick = onClick;

  // NOTE: flow does not know about oncontextmenu
  (bp: any).oncontextmenu = onContextMenu;

  return bp;
}

export default class ColumnBreakpoint extends PureComponent<Props> {
  addColumnBreakpoint: Function;
  bookmark: ?Bookmark;

  addColumnBreakpoint = (nextProps: ?Props) => {
    const { columnBreakpoint, source } = nextProps || this.props;

    const sourceId = source.id;
    const doc = getDocument(sourceId);
    if (!doc) {
      return;
    }

    const { line, column } = columnBreakpoint.location;
    const widget = makeBookmark(columnBreakpoint, {
      onClick: this.onClick,
      onContextMenu: this.onContextMenu
    });

    this.bookmark = doc.setBookmark({ line: line - 1, ch: column }, { widget });
  };

  clearColumnBreakpoint = () => {
    if (this.bookmark) {
      this.bookmark.clear();
      this.bookmark = null;
    }
  };

  onClick = (event: MouseEvent) => {
    event.stopPropagation();
    event.preventDefault();
    const { columnBreakpoint, breakpointActions } = this.props;
    if (columnBreakpoint.breakpoint) {
      breakpointActions.removeBreakpoint(columnBreakpoint.breakpoint);
    } else {
      breakpointActions.addBreakpoint(columnBreakpoint.location);
    }
  };

  onContextMenu = (event: MouseEvent) => {
    event.stopPropagation();
    event.preventDefault();
    const {
      columnBreakpoint: { breakpoint, location },
      breakpointActions
    } = this.props;

    const items = breakpoint
      ? breakpointItems(breakpoint, breakpointActions)
      : createBreakpointItems(location, breakpointActions);

    showMenu(event, items);
  };

  componentDidMount() {
    this.addColumnBreakpoint();
  }

  componentWillUnmount() {
    this.clearColumnBreakpoint();
  }

  componentDidUpdate() {
    this.clearColumnBreakpoint();
    this.addColumnBreakpoint();
  }

  render() {
    return null;
  }
}
