/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu } from "../../context-menu/menu";
import { getSelectedLocation } from "../../utils/selected-location";
import { features } from "../../utils/prefs";
import { formatKeyShortcut } from "../../utils/text";
import { isLineBlackboxed } from "../../utils/source";

import {
  getSelectedSource,
  getBlackBoxRanges,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";
import {
  addBreakpoint,
  removeBreakpoint,
  setBreakpointOptions,
} from "../../actions/breakpoints/modify";
import {
  enableBreakpointsAtLine,
  disableBreakpointsAtLine,
  toggleDisabledBreakpoint,
  removeBreakpointsAtLine,
} from "../../actions/breakpoints/index";
import { openConditionalPanel } from "../../actions/ui";

export function showEditorEditBreakpointContextMenu(event, breakpoint) {
  return async ({ dispatch, getState }) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);
    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const blackboxedRanges = getBlackBoxRanges(state);
    const blackboxedRangesForSelectedSource =
      blackboxedRanges[selectedSource.url];
    const isSelectedSourceOnIgnoreList =
      selectedSource &&
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, selectedSource);

    const items = [
      removeBreakpointItem(breakpoint, dispatch),
      toggleDisabledBreakpointItem(
        breakpoint,
        blackboxedRangesForSelectedSource,
        isSelectedSourceOnIgnoreList,
        dispatch
      ),
    ];

    if (breakpoint.originalText.startsWith("debugger")) {
      items.push(
        { type: "separator" },
        toggleDbgStatementItem(selectedLocation, breakpoint, dispatch)
      );
    }

    items.push(
      { type: "separator" },
      removeBreakpointsOnLineItem(selectedLocation, dispatch),
      breakpoint.disabled
        ? enableBreakpointsOnLineItem(
            selectedLocation,
            blackboxedRangesForSelectedSource,
            isSelectedSourceOnIgnoreList,
            dispatch
          )
        : disableBreakpointsOnLineItem(selectedLocation, dispatch),
      { type: "separator" }
    );

    items.push(
      conditionalBreakpointItem(breakpoint, selectedLocation, dispatch)
    );
    items.push(logPointItem(breakpoint, selectedLocation, dispatch));

    showMenu(event, items);
  };
}

export function showEditorCreateBreakpointContextMenu(
  event,
  location,
  lineText
) {
  return async ({ dispatch, getState }) => {
    const items = createBreakpointItems(location, lineText, dispatch);

    showMenu(event, items);
  };
}

export function createBreakpointItems(location, lineText, dispatch) {
  const items = [
    addBreakpointItem(location, dispatch),
    addConditionalBreakpointItem(location, dispatch),
  ];

  if (features.logPoints) {
    items.push(addLogPointItem(location, dispatch));
  }

  if (lineText && lineText.startsWith("debugger")) {
    items.push(toggleDbgStatementItem(location, null, dispatch));
  }
  return items;
}

const addBreakpointItem = (location, dispatch) => ({
  id: "node-menu-add-breakpoint",
  label: L10N.getStr("editor.addBreakpoint"),
  accesskey: L10N.getStr("shortcuts.toggleBreakpoint.accesskey"),
  disabled: false,
  click: () => dispatch(addBreakpoint(location)),
  accelerator: formatKeyShortcut(L10N.getStr("toggleBreakpoint.key")),
});

const removeBreakpointItem = (breakpoint, dispatch) => ({
  id: "node-menu-remove-breakpoint",
  label: L10N.getStr("editor.removeBreakpoint"),
  accesskey: L10N.getStr("shortcuts.toggleBreakpoint.accesskey"),
  disabled: false,
  click: () => dispatch(removeBreakpoint(breakpoint)),
  accelerator: formatKeyShortcut(L10N.getStr("toggleBreakpoint.key")),
});

const addConditionalBreakpointItem = (location, dispatch) => ({
  id: "node-menu-add-conditional-breakpoint",
  label: L10N.getStr("editor.addConditionBreakpoint"),
  accelerator: formatKeyShortcut(L10N.getStr("toggleCondPanel.breakpoint.key")),
  accesskey: L10N.getStr("editor.addConditionBreakpoint.accesskey"),
  disabled: false,
  click: () => dispatch(openConditionalPanel(location)),
});

const editConditionalBreakpointItem = (location, dispatch) => ({
  id: "node-menu-edit-conditional-breakpoint",
  label: L10N.getStr("editor.editConditionBreakpoint"),
  accelerator: formatKeyShortcut(L10N.getStr("toggleCondPanel.breakpoint.key")),
  accesskey: L10N.getStr("editor.addConditionBreakpoint.accesskey"),
  disabled: false,
  click: () => dispatch(openConditionalPanel(location)),
});

const conditionalBreakpointItem = (breakpoint, location, dispatch) => {
  const {
    options: { condition },
  } = breakpoint;
  return condition
    ? editConditionalBreakpointItem(location, dispatch)
    : addConditionalBreakpointItem(location, dispatch);
};

const addLogPointItem = (location, dispatch) => ({
  id: "node-menu-add-log-point",
  label: L10N.getStr("editor.addLogPoint"),
  accesskey: L10N.getStr("editor.addLogPoint.accesskey"),
  disabled: false,
  click: () => dispatch(openConditionalPanel(location, true)),
  accelerator: formatKeyShortcut(L10N.getStr("toggleCondPanel.logPoint.key")),
});

const editLogPointItem = (location, dispatch) => ({
  id: "node-menu-edit-log-point",
  label: L10N.getStr("editor.editLogPoint"),
  accesskey: L10N.getStr("editor.editLogPoint.accesskey"),
  disabled: false,
  click: () => dispatch(openConditionalPanel(location, true)),
  accelerator: formatKeyShortcut(L10N.getStr("toggleCondPanel.logPoint.key")),
});

const logPointItem = (breakpoint, location, dispatch) => {
  const {
    options: { logValue },
  } = breakpoint;
  return logValue
    ? editLogPointItem(location, dispatch)
    : addLogPointItem(location, dispatch);
};

const toggleDisabledBreakpointItem = (
  breakpoint,
  blackboxedRangesForSelectedSource,
  isSelectedSourceOnIgnoreList,
  dispatch
) => {
  return {
    accesskey: L10N.getStr("editor.disableBreakpoint.accesskey"),
    disabled: isLineBlackboxed(
      blackboxedRangesForSelectedSource,
      breakpoint.location.line,
      isSelectedSourceOnIgnoreList
    ),
    click: () => dispatch(toggleDisabledBreakpoint(breakpoint)),
    ...(breakpoint.disabled
      ? {
          id: "node-menu-enable-breakpoint",
          label: L10N.getStr("editor.enableBreakpoint"),
        }
      : {
          id: "node-menu-disable-breakpoint",
          label: L10N.getStr("editor.disableBreakpoint"),
        }),
  };
};

const toggleDbgStatementItem = (location, breakpoint, dispatch) => {
  if (breakpoint && breakpoint.options.condition === "false") {
    return {
      disabled: false,
      id: "node-menu-enable-dbgStatement",
      label: L10N.getStr("breakpointMenuItem.enabledbg.label"),
      click: () =>
        dispatch(
          setBreakpointOptions(location, {
            ...breakpoint.options,
            condition: null,
          })
        ),
    };
  }

  return {
    disabled: false,
    id: "node-menu-disable-dbgStatement",
    label: L10N.getStr("breakpointMenuItem.disabledbg.label"),
    click: () =>
      dispatch(
        setBreakpointOptions(location, {
          condition: "false",
        })
      ),
  };
};

// ToDo: Only enable if there are more than one breakpoints on a line?
const removeBreakpointsOnLineItem = (location, dispatch) => ({
  id: "node-menu-remove-breakpoints-on-line",
  label: L10N.getStr("breakpointMenuItem.removeAllAtLine.label"),
  accesskey: L10N.getStr("breakpointMenuItem.removeAllAtLine.accesskey"),
  disabled: false,
  click: () =>
    dispatch(removeBreakpointsAtLine(location.source, location.line)),
});

const enableBreakpointsOnLineItem = (
  location,
  blackboxedRangesForSelectedSource,
  isSelectedSourceOnIgnoreList,
  dispatch
) => ({
  id: "node-menu-remove-breakpoints-on-line",
  label: L10N.getStr("breakpointMenuItem.enableAllAtLine.label"),
  accesskey: L10N.getStr("breakpointMenuItem.enableAllAtLine.accesskey"),
  disabled: isLineBlackboxed(
    blackboxedRangesForSelectedSource,
    location.line,
    isSelectedSourceOnIgnoreList
  ),
  click: () =>
    dispatch(enableBreakpointsAtLine(location.source, location.line)),
});

const disableBreakpointsOnLineItem = (location, dispatch) => ({
  id: "node-menu-remove-breakpoints-on-line",
  label: L10N.getStr("breakpointMenuItem.disableAllAtLine.label"),
  accesskey: L10N.getStr("breakpointMenuItem.disableAllAtLine.accesskey"),
  disabled: false,
  click: () =>
    dispatch(disableBreakpointsAtLine(location.source, location.line)),
});
