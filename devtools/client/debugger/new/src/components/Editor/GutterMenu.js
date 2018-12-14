/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { showMenu } from "devtools-contextmenu";
import { connect } from "react-redux";
import { lineAtHeight } from "../../utils/editor";
import {
  getContextMenu,
  getEmptyLines,
  getSelectedLocation,
  getSelectedSource,
  getVisibleBreakpoints,
  isPaused as getIsPaused
} from "../../selectors";

import actions from "../../actions";

type Props = {
  setContextMenu: Function,
  contextMenu: Object
};

export function gutterMenu({
  breakpoint,
  line,
  column,
  event,
  isPaused,
  toggleBreakpoint,
  openConditionalPanel,
  toggleDisabledBreakpoint,
  isCbPanelOpen,
  closeConditionalPanel,
  continueToHere,
  sourceId
}) {
  event.stopPropagation();
  event.preventDefault();

  const gutterItems = {
    addBreakpoint: {
      id: "node-menu-add-breakpoint",
      label: L10N.getStr("editor.addBreakpoint")
    },
    addConditional: {
      id: "node-menu-add-conditional-breakpoint",
      label: L10N.getStr("editor.addConditionalBreakpoint")
    },
    removeBreakpoint: {
      id: "node-menu-remove-breakpoint",
      label: L10N.getStr("editor.removeBreakpoint")
    },
    editConditional: {
      id: "node-menu-edit-conditional-breakpoint",
      label: L10N.getStr("editor.editBreakpoint")
    },
    enableBreakpoint: {
      id: "node-menu-enable-breakpoint",
      label: L10N.getStr("editor.enableBreakpoint")
    },
    disableBreakpoint: {
      id: "node-menu-disable-breakpoint",
      label: L10N.getStr("editor.disableBreakpoint")
    },
    continueToHere: {
      id: "node-menu-continue-to-here",
      label: L10N.getStr("editor.continueToHere.label")
    }
  };

  const toggleBreakpointItem = {
    accesskey: L10N.getStr("shortcuts.toggleBreakpoint.accesskey"),
    disabled: false,
    click: () => {
      toggleBreakpoint(line, column);
      if (isCbPanelOpen) {
        closeConditionalPanel();
      }
    },
    accelerator: L10N.getStr("toggleBreakpoint.key"),
    ...(breakpoint ? gutterItems.removeBreakpoint : gutterItems.addBreakpoint)
  };

  const conditionalBreakpoint = {
    accesskey: L10N.getStr("editor.addConditionalBreakpoint.accesskey"),
    disabled: false,
    // Leaving column undefined so pause points can be detected
    click: () =>
      openConditionalPanel(
        breakpoint ? breakpoint.location : { line, column, sourceId }
      ),
    accelerator: L10N.getStr("toggleCondPanel.key"),
    ...(breakpoint && breakpoint.condition
      ? gutterItems.editConditional
      : gutterItems.addConditional)
  };

  const items = [toggleBreakpointItem, conditionalBreakpoint];

  if (isPaused) {
    const continueToHereItem = {
      accesskey: L10N.getStr("editor.continueToHere.accesskey"),
      disabled: false,
      click: () => continueToHere(line, column),
      ...gutterItems.continueToHere
    };
    items.push(continueToHereItem);
  }

  if (breakpoint) {
    const disableBreakpoint = {
      accesskey: L10N.getStr("editor.disableBreakpoint.accesskey"),
      disabled: false,
      click: () => toggleDisabledBreakpoint(line, column),
      ...(breakpoint.disabled
        ? gutterItems.enableBreakpoint
        : gutterItems.disableBreakpoint)
    };
    items.push(disableBreakpoint);
  }

  showMenu(event, items);
}

class GutterContextMenuComponent extends Component {
  props: Props;

  shouldComponentUpdate(nextProps) {
    return nextProps.contextMenu.type === "Gutter";
  }

  componentWillUpdate(nextProps) {
    // clear the context menu since it is open
    this.props.setContextMenu("", null);
    return this.showMenu(nextProps);
  }

  showMenu(nextProps) {
    const { contextMenu, ...props } = nextProps;
    const { event } = contextMenu;

    const sourceId = props.selectedSource ? props.selectedSource.id : "";
    const line = lineAtHeight(props.editor, sourceId, event);
    const column = props.editor.codeMirror.coordsChar({
      left: event.x,
      top: event.y
    }).ch;
    const breakpoint = nextProps.breakpoints.find(
      bp => bp.location.line === line && bp.location.column === column
    );

    if (props.emptyLines && props.emptyLines.includes(line)) {
      return;
    }

    gutterMenu({ event, sourceId, line, column, breakpoint, ...props });
  }

  render() {
    return null;
  }
}

const mapStateToProps = state => {
  const selectedSource = getSelectedSource(state);

  return {
    selectedLocation: getSelectedLocation(state),
    selectedSource: selectedSource,
    breakpoints: getVisibleBreakpoints(state),
    isPaused: getIsPaused(state),
    contextMenu: getContextMenu(state),
    emptyLines: getEmptyLines(state, selectedSource.id)
  };
};

export default connect(
  mapStateToProps,
  actions
)(GutterContextMenuComponent);
