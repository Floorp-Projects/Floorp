/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  INITIALIZE,
  MESSAGES_CLEAR,
  PERSIST_TOGGLE,
  REVERSE_SEARCH_INPUT_TOGGLE,
  SELECT_NETWORK_MESSAGE_TAB,
  SHOW_CONTENT_MESSAGES_TOGGLE,
  SHOW_OBJECT_IN_SIDEBAR,
  SIDEBAR_CLOSE,
  SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
  TIMESTAMPS_TOGGLE,
  FILTERBAR_DISPLAY_MODE_SET,
  FILTERBAR_DISPLAY_MODES,
  EDITOR_ONBOARDING_DISMISS,
  EDITOR_TOGGLE,
  EDITOR_SET_WIDTH,
} = require("devtools/client/webconsole/constants");

const { PANELS } = require("devtools/client/netmonitor/src/constants");

const UiState = overrides =>
  Object.freeze(
    Object.assign(
      {
        initialized: false,
        networkMessageActiveTabId: PANELS.HEADERS,
        persistLogs: false,
        showContentMessages: false,
        sidebarVisible: false,
        timestampsVisible: true,
        gripInSidebar: null,
        closeButtonVisible: false,
        reverseSearchInputVisible: false,
        reverseSearchInitialValue: "",
        editor: false,
        editorWidth: null,
        showEditorOnboarding: false,
        filterBarDisplayMode: FILTERBAR_DISPLAY_MODES.WIDE,
      },
      overrides
    )
  );

function ui(state = UiState(), action) {
  switch (action.type) {
    case PERSIST_TOGGLE:
      return { ...state, persistLogs: !state.persistLogs };
    case SHOW_CONTENT_MESSAGES_TOGGLE:
      return { ...state, showContentMessages: !state.showContentMessages };
    case TIMESTAMPS_TOGGLE:
      return { ...state, timestampsVisible: action.visible };
    case SELECT_NETWORK_MESSAGE_TAB:
      return { ...state, networkMessageActiveTabId: action.id };
    case SIDEBAR_CLOSE:
      return {
        ...state,
        sidebarVisible: false,
        gripInSidebar: null,
      };
    case INITIALIZE:
      return { ...state, initialized: true };
    case MESSAGES_CLEAR:
      return { ...state, sidebarVisible: false, gripInSidebar: null };
    case SHOW_OBJECT_IN_SIDEBAR:
      if (action.grip === state.gripInSidebar) {
        return state;
      }
      return { ...state, sidebarVisible: true, gripInSidebar: action.grip };
    case SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE:
      return { ...state, closeButtonVisible: action.shouldDisplayButton };
    case REVERSE_SEARCH_INPUT_TOGGLE:
      return {
        ...state,
        reverseSearchInputVisible: !state.reverseSearchInputVisible,
        reverseSearchInitialValue: action.initialValue || "",
      };
    case FILTERBAR_DISPLAY_MODE_SET:
      return {
        ...state,
        filterBarDisplayMode: action.displayMode,
      };
    case EDITOR_TOGGLE:
      return {
        ...state,
        editor: !state.editor,
      };
    case EDITOR_ONBOARDING_DISMISS:
      return {
        ...state,
        showEditorOnboarding: false,
      };
    case EDITOR_SET_WIDTH:
      return {
        ...state,
        editorWidth: action.width,
      };
  }

  return state;
}

module.exports = {
  UiState,
  ui,
};
