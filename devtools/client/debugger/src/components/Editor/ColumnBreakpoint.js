/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { PureComponent } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";
import { showMenu } from "../../context-menu/menu";

import { getDocument } from "../../utils/editor";
import { breakpointItems, createBreakpointItems } from "./menus/breakpoints";
import { getSelectedLocation } from "../../utils/selected-location";

// eslint-disable-next-line max-len

const breakpointButton = document.createElement("button");
breakpointButton.innerHTML =
  '<svg viewBox="0 0 11 13" width="11" height="13"><path d="M5.07.5H1.5c-.54 0-1 .46-1 1v10c0 .54.46 1 1 1h3.57c.58 0 1.15-.26 1.53-.7l3.7-5.3-3.7-5.3C6.22.76 5.65.5 5.07.5z"/></svg>';

function makeBookmark({ breakpoint }, { onClick, onContextMenu }) {
  const bp = breakpointButton.cloneNode(true);

  const isActive = breakpoint && !breakpoint.disabled;
  const isDisabled = breakpoint?.disabled;
  const condition = breakpoint?.options.condition;
  const logValue = breakpoint?.options.logValue;

  bp.className = classnames("column-breakpoint", {
    "has-condition": condition,
    "has-log": logValue,
    active: isActive,
    disabled: isDisabled,
  });

  bp.setAttribute("title", logValue || condition || "");
  bp.onclick = onClick;
  bp.oncontextmenu = onContextMenu;

  return bp;
}

export default class ColumnBreakpoint extends PureComponent {
  bookmark;

  static get propTypes() {
    return {
      breakpointActions: PropTypes.object.isRequired,
      columnBreakpoint: PropTypes.object.isRequired,
      cx: PropTypes.object.isRequired,
      source: PropTypes.object.isRequired,
    };
  }

  addColumnBreakpoint = nextProps => {
    const { columnBreakpoint, source } = nextProps || this.props;

    const sourceId = source.id;
    const doc = getDocument(sourceId);
    if (!doc) {
      return;
    }

    const { line, column } = columnBreakpoint.location;
    const widget = makeBookmark(columnBreakpoint, {
      onClick: this.onClick,
      onContextMenu: this.onContextMenu,
    });

    this.bookmark = doc.setBookmark({ line: line - 1, ch: column }, { widget });
  };

  clearColumnBreakpoint = () => {
    if (this.bookmark) {
      this.bookmark.clear();
      this.bookmark = null;
    }
  };

  onClick = event => {
    event.stopPropagation();
    event.preventDefault();
    const { cx, columnBreakpoint, breakpointActions } = this.props;

    // disable column breakpoint on shift-click.
    if (event.shiftKey) {
      const breakpoint = columnBreakpoint.breakpoint;
      breakpointActions.toggleDisabledBreakpoint(cx, breakpoint);
      return;
    }

    if (columnBreakpoint.breakpoint) {
      breakpointActions.removeBreakpoint(cx, columnBreakpoint.breakpoint);
    } else {
      breakpointActions.addBreakpoint(cx, columnBreakpoint.location);
    }
  };

  onContextMenu = event => {
    event.stopPropagation();
    event.preventDefault();
    const {
      cx,
      columnBreakpoint: { breakpoint, location },
      source,
      breakpointActions,
    } = this.props;

    let items = createBreakpointItems(cx, location, breakpointActions);

    if (breakpoint) {
      const selectedLocation = getSelectedLocation(breakpoint, source);

      items = breakpointItems(
        cx,
        breakpoint,
        selectedLocation,
        breakpointActions
      );
    }

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
