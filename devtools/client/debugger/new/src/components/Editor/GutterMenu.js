/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { showMenu } from "devtools-contextmenu";
import { connect } from "../../utils/connect";
import { lineAtHeight } from "../../utils/editor";
import { features } from "../../utils/prefs";
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
    addLogPoint: {
      id: "node-menu-add-log-point",
      label: L10N.getStr("editor.addLogPoint")
    },
    addConditional: {
      id: "node-menu-add-conditional-breakpoint",
      label: L10N.getStr("editor.addConditionBreakpoint")
    },
    removeBreakpoint: {
      id: "node-menu-remove-breakpoint",
      label: L10N.getStr("editor.removeBreakpoint")
    },
    editLogPoint: {
      id: "node-menu-edit-log-point",
      label: L10N.getStr("editor.editLogPoint")
    },
    editConditional: {
      id: "node-menu-edit-conditional-breakpoint",
      label: L10N.getStr("editor.editConditionBreakpoint")
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

  const logPoint = {
    accesskey: L10N.getStr("editor.addLogPoint.accesskey"),
    disabled: false,
    click: () =>
      openConditionalPanel(
        breakpoint ? breakpoint.location : { line, column, sourceId },
        true
      ),
    accelerator: L10N.getStr("toggleCondPanel.key"),
    ...(breakpoint && breakpoint.condition
      ? gutterItems.editLogPoint
      : gutterItems.addLogPoint)
  };

  const conditionalBreakpoint = {
    accesskey: L10N.getStr("editor.addConditionBreakpoint.accesskey"),
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

  let items = [toggleBreakpointItem, conditionalBreakpoint];
  
  if (features.logPoints) {
    items.push(logPoint)
  }

  if (breakpoint && breakpoint.condition) {
    const remove = breakpoint.log ? conditionalBreakpoint : logPoint;
    items = items.filter(item => item !== remove);
  }

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

    // Allow getFirstVisiblePausePoint to find the best first breakpoint
    // position by not providing an explicit column number
    if (features.columnBreakpoints && !breakpoint && column === 0) {
      column = undefined;
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
