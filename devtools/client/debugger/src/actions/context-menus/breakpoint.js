/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { buildMenu, showMenu } from "../../context-menu/menu";
import { getSelectedLocation } from "../../utils/selected-location";
import { isLineBlackboxed } from "../../utils/source";
import { features } from "../../utils/prefs";
import { formatKeyShortcut } from "../../utils/text";

import {
  getBreakpointsList,
  getSelectedSource,
  getBlackBoxRanges,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";

import {
  removeBreakpoint,
  setBreakpointOptions,
} from "../../actions/breakpoints/modify";
import {
  removeBreakpoints,
  removeAllBreakpoints,
  toggleBreakpoints,
  toggleAllBreakpoints,
  toggleDisabledBreakpoint,
} from "../../actions/breakpoints/index";
import { selectSpecificLocation } from "../../actions/sources/select";
import { openConditionalPanel } from "../../actions/ui";

export function showBreakpointContextMenu(event, breakpoint, source) {
  return async ({ dispatch, getState }) => {
    const state = getState();
    const breakpoints = getBreakpointsList(state);
    const blackboxedRanges = getBlackBoxRanges(state);
    const blackboxedRangesForSource = blackboxedRanges[source.url];
    const checkSourceOnIgnoreList = _source =>
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, _source);
    const selectedSource = getSelectedSource(state);

    const deleteSelfLabel = L10N.getStr("breakpointMenuItem.deleteSelf2.label");
    const deleteAllLabel = L10N.getStr("breakpointMenuItem.deleteAll2.label");
    const deleteOthersLabel = L10N.getStr(
      "breakpointMenuItem.deleteOthers2.label"
    );
    const enableSelfLabel = L10N.getStr("breakpointMenuItem.enableSelf2.label");
    const enableAllLabel = L10N.getStr("breakpointMenuItem.enableAll2.label");
    const enableOthersLabel = L10N.getStr(
      "breakpointMenuItem.enableOthers2.label"
    );
    const disableSelfLabel = L10N.getStr(
      "breakpointMenuItem.disableSelf2.label"
    );
    const disableAllLabel = L10N.getStr("breakpointMenuItem.disableAll2.label");
    const disableOthersLabel = L10N.getStr(
      "breakpointMenuItem.disableOthers2.label"
    );
    const enableDbgStatementLabel = L10N.getStr(
      "breakpointMenuItem.enabledbg.label"
    );
    const disableDbgStatementLabel = L10N.getStr(
      "breakpointMenuItem.disabledbg.label"
    );
    const removeConditionLabel = L10N.getStr(
      "breakpointMenuItem.removeCondition2.label"
    );
    const addConditionLabel = L10N.getStr(
      "breakpointMenuItem.addCondition2.label"
    );
    const editConditionLabel = L10N.getStr(
      "breakpointMenuItem.editCondition2.label"
    );

    const deleteSelfKey = L10N.getStr(
      "breakpointMenuItem.deleteSelf2.accesskey"
    );
    const deleteAllKey = L10N.getStr("breakpointMenuItem.deleteAll2.accesskey");
    const deleteOthersKey = L10N.getStr(
      "breakpointMenuItem.deleteOthers2.accesskey"
    );
    const enableSelfKey = L10N.getStr(
      "breakpointMenuItem.enableSelf2.accesskey"
    );
    const enableAllKey = L10N.getStr("breakpointMenuItem.enableAll2.accesskey");
    const enableOthersKey = L10N.getStr(
      "breakpointMenuItem.enableOthers2.accesskey"
    );
    const disableSelfKey = L10N.getStr(
      "breakpointMenuItem.disableSelf2.accesskey"
    );
    const disableAllKey = L10N.getStr(
      "breakpointMenuItem.disableAll2.accesskey"
    );
    const disableOthersKey = L10N.getStr(
      "breakpointMenuItem.disableOthers2.accesskey"
    );
    const removeConditionKey = L10N.getStr(
      "breakpointMenuItem.removeCondition2.accesskey"
    );
    const editConditionKey = L10N.getStr(
      "breakpointMenuItem.editCondition2.accesskey"
    );
    const addConditionKey = L10N.getStr(
      "breakpointMenuItem.addCondition2.accesskey"
    );

    const selectedLocation = getSelectedLocation(breakpoint, selectedSource);
    const otherBreakpoints = breakpoints.filter(b => b.id !== breakpoint.id);
    const enabledBreakpoints = breakpoints.filter(b => !b.disabled);
    const disabledBreakpoints = breakpoints.filter(b => b.disabled);
    const otherEnabledBreakpoints = breakpoints.filter(
      b => !b.disabled && b.id !== breakpoint.id
    );
    const otherDisabledBreakpoints = breakpoints.filter(
      b => b.disabled && b.id !== breakpoint.id
    );

    const deleteSelfItem = {
      id: "node-menu-delete-self",
      label: deleteSelfLabel,
      accesskey: deleteSelfKey,
      disabled: false,
      click: () => {
        dispatch(removeBreakpoint(breakpoint));
      },
    };

    const deleteAllItem = {
      id: "node-menu-delete-all",
      label: deleteAllLabel,
      accesskey: deleteAllKey,
      disabled: false,
      click: () => dispatch(removeAllBreakpoints()),
    };

    const deleteOthersItem = {
      id: "node-menu-delete-other",
      label: deleteOthersLabel,
      accesskey: deleteOthersKey,
      disabled: false,
      click: () => dispatch(removeBreakpoints(otherBreakpoints)),
    };

    const enableSelfItem = {
      id: "node-menu-enable-self",
      label: enableSelfLabel,
      accesskey: enableSelfKey,
      disabled: isLineBlackboxed(
        blackboxedRangesForSource,
        breakpoint.location.line,
        checkSourceOnIgnoreList(breakpoint.location.source)
      ),
      click: () => {
        dispatch(toggleDisabledBreakpoint(breakpoint));
      },
    };

    const enableAllItem = {
      id: "node-menu-enable-all",
      label: enableAllLabel,
      accesskey: enableAllKey,
      disabled: isLineBlackboxed(
        blackboxedRangesForSource,
        breakpoint.location.line,
        checkSourceOnIgnoreList(breakpoint.location.source)
      ),
      click: () => dispatch(toggleAllBreakpoints(false)),
    };

    const enableOthersItem = {
      id: "node-menu-enable-others",
      label: enableOthersLabel,
      accesskey: enableOthersKey,
      disabled: isLineBlackboxed(
        blackboxedRangesForSource,
        breakpoint.location.line,
        checkSourceOnIgnoreList(breakpoint.location.source)
      ),
      click: () => dispatch(toggleBreakpoints(false, otherDisabledBreakpoints)),
    };

    const disableSelfItem = {
      id: "node-menu-disable-self",
      label: disableSelfLabel,
      accesskey: disableSelfKey,
      disabled: false,
      click: () => {
        dispatch(toggleDisabledBreakpoint(breakpoint));
      },
    };

    const disableAllItem = {
      id: "node-menu-disable-all",
      label: disableAllLabel,
      accesskey: disableAllKey,
      disabled: false,
      click: () => dispatch(toggleAllBreakpoints(true)),
    };

    const disableOthersItem = {
      id: "node-menu-disable-others",
      label: disableOthersLabel,
      accesskey: disableOthersKey,
      click: () => dispatch(toggleBreakpoints(true, otherEnabledBreakpoints)),
    };

    const enableDbgStatementItem = {
      id: "node-menu-enable-dbgStatement",
      label: enableDbgStatementLabel,
      disabled: false,
      click: () =>
        dispatch(
          setBreakpointOptions(selectedLocation, {
            ...breakpoint.options,
            condition: null,
          })
        ),
    };

    const disableDbgStatementItem = {
      id: "node-menu-disable-dbgStatement",
      label: disableDbgStatementLabel,
      disabled: false,
      click: () =>
        dispatch(
          setBreakpointOptions(selectedLocation, {
            ...breakpoint.options,
            condition: "false",
          })
        ),
    };

    const removeConditionItem = {
      id: "node-menu-remove-condition",
      label: removeConditionLabel,
      accesskey: removeConditionKey,
      disabled: false,
      click: () =>
        dispatch(
          setBreakpointOptions(selectedLocation, {
            ...breakpoint.options,
            condition: null,
          })
        ),
    };

    const addConditionItem = {
      id: "node-menu-add-condition",
      label: addConditionLabel,
      accesskey: addConditionKey,
      click: async () => {
        await dispatch(selectSpecificLocation(selectedLocation));
        await dispatch(openConditionalPanel(selectedLocation));
      },
      accelerator: formatKeyShortcut(
        L10N.getStr("toggleCondPanel.breakpoint.key")
      ),
    };

    const editConditionItem = {
      id: "node-menu-edit-condition",
      label: editConditionLabel,
      accesskey: editConditionKey,
      click: async () => {
        await dispatch(selectSpecificLocation(selectedLocation));
        await dispatch(openConditionalPanel(selectedLocation));
      },
      accelerator: formatKeyShortcut(
        L10N.getStr("toggleCondPanel.breakpoint.key")
      ),
    };

    const addLogPointItem = {
      id: "node-menu-add-log-point",
      label: L10N.getStr("editor.addLogPoint"),
      accesskey: L10N.getStr("editor.addLogPoint.accesskey"),
      disabled: false,
      click: async () => {
        await dispatch(selectSpecificLocation(selectedLocation));
        await dispatch(openConditionalPanel(selectedLocation, true));
      },
      accelerator: formatKeyShortcut(
        L10N.getStr("toggleCondPanel.logPoint.key")
      ),
    };

    const editLogPointItem = {
      id: "node-menu-edit-log-point",
      label: L10N.getStr("editor.editLogPoint"),
      accesskey: L10N.getStr("editor.editLogPoint.accesskey"),
      disabled: false,
      click: async () => {
        await dispatch(selectSpecificLocation(selectedLocation));
        await dispatch(openConditionalPanel(selectedLocation, true));
      },
      accelerator: formatKeyShortcut(
        L10N.getStr("toggleCondPanel.logPoint.key")
      ),
    };

    const removeLogPointItem = {
      id: "node-menu-remove-log",
      label: L10N.getStr("editor.removeLogPoint.label"),
      accesskey: L10N.getStr("editor.removeLogPoint.accesskey"),
      disabled: false,
      click: () =>
        dispatch(
          setBreakpointOptions(selectedLocation, {
            ...breakpoint.options,
            logValue: null,
          })
        ),
    };

    const logPointItem = breakpoint.options.logValue
      ? editLogPointItem
      : addLogPointItem;

    const hideEnableSelfItem = !breakpoint.disabled;
    const hideEnableAllItem = disabledBreakpoints.length === 0;
    const hideEnableOthersItem = otherDisabledBreakpoints.length === 0;
    const hideDisableAllItem = enabledBreakpoints.length === 0;
    const hideDisableOthersItem = otherEnabledBreakpoints.length === 0;
    const hideDisableSelfItem = breakpoint.disabled;
    const hideEnableDbgStatementItem =
      !breakpoint.originalText.startsWith("debugger") ||
      (breakpoint.originalText.startsWith("debugger") &&
        breakpoint.options.condition !== "false");
    const hideDisableDbgStatementItem =
      !breakpoint.originalText.startsWith("debugger") ||
      (breakpoint.originalText.startsWith("debugger") &&
        breakpoint.options.condition === "false");
    const items = [
      { item: enableSelfItem, hidden: () => hideEnableSelfItem },
      { item: enableAllItem, hidden: () => hideEnableAllItem },
      { item: enableOthersItem, hidden: () => hideEnableOthersItem },
      {
        item: { type: "separator" },
        hidden: () =>
          hideEnableSelfItem && hideEnableAllItem && hideEnableOthersItem,
      },
      { item: deleteSelfItem },
      { item: deleteAllItem },
      { item: deleteOthersItem, hidden: () => breakpoints.length === 1 },
      {
        item: { type: "separator" },
        hidden: () =>
          hideDisableSelfItem && hideDisableAllItem && hideDisableOthersItem,
      },

      { item: disableSelfItem, hidden: () => hideDisableSelfItem },
      { item: disableAllItem, hidden: () => hideDisableAllItem },
      { item: disableOthersItem, hidden: () => hideDisableOthersItem },
      {
        item: { type: "separator" },
      },
      {
        item: enableDbgStatementItem,
        hidden: () => hideEnableDbgStatementItem,
      },
      {
        item: disableDbgStatementItem,
        hidden: () => hideDisableDbgStatementItem,
      },
      {
        item: { type: "separator" },
        hidden: () => hideDisableDbgStatementItem && hideEnableDbgStatementItem,
      },
      {
        item: addConditionItem,
        hidden: () => breakpoint.options.condition,
      },
      {
        item: editConditionItem,
        hidden: () => !breakpoint.options.condition,
      },
      {
        item: removeConditionItem,
        hidden: () => !breakpoint.options.condition,
      },
      {
        item: logPointItem,
        hidden: () => !features.logPoints,
      },
      {
        item: removeLogPointItem,
        hidden: () => !features.logPoints || !breakpoint.options.logValue,
      },
    ];

    showMenu(event, buildMenu(items));
  };
}
