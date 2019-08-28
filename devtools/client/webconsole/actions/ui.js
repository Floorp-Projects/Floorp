/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllUi } = require("devtools/client/webconsole/selectors/ui");
const { getMessage } = require("devtools/client/webconsole/selectors/messages");

const {
  INITIALIZE,
  PERSIST_TOGGLE,
  PREFS,
  REVERSE_SEARCH_INPUT_TOGGLE,
  SELECT_NETWORK_MESSAGE_TAB,
  SHOW_CONTENT_MESSAGES_TOGGLE,
  SHOW_OBJECT_IN_SIDEBAR,
  SIDEBAR_CLOSE,
  SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
  TIMESTAMPS_TOGGLE,
  WARNING_GROUPS_TOGGLE,
  FILTERBAR_DISPLAY_MODE_SET,
  EDITOR_TOGGLE,
  EDITOR_SET_WIDTH,
  EDITOR_ONBOARDING_DISMISS,
} = require("devtools/client/webconsole/constants");

function persistToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: PERSIST_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.PERSIST, uiState.persistLogs);
  };
}

function contentMessagesToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: SHOW_CONTENT_MESSAGES_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(
      PREFS.UI.CONTENT_MESSAGES,
      uiState.showContentMessages
    );
  };
}

function timestampsToggle(visible) {
  return {
    type: TIMESTAMPS_TOGGLE,
    visible,
  };
}

function warningGroupsToggle(value) {
  return {
    type: WARNING_GROUPS_TOGGLE,
    value,
  };
}

function selectNetworkMessageTab(id) {
  return {
    type: SELECT_NETWORK_MESSAGE_TAB,
    id,
  };
}

function initialize() {
  return {
    type: INITIALIZE,
  };
}

function sidebarClose() {
  return {
    type: SIDEBAR_CLOSE,
  };
}

function splitConsoleCloseButtonToggle(shouldDisplayButton) {
  return {
    type: SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
    shouldDisplayButton,
  };
}

function editorToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: EDITOR_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.EDITOR, uiState.editor);
  };
}

function editorOnboardingDismiss() {
  return ({ dispatch, prefsService }) => {
    dispatch({
      type: EDITOR_ONBOARDING_DISMISS,
    });
    prefsService.setBoolPref(PREFS.UI.EDITOR_ONBOARDING, false);
  };
}

function setEditorWidth(width) {
  return ({ dispatch, prefsService }) => {
    dispatch({
      type: EDITOR_SET_WIDTH,
      width,
    });
    prefsService.setIntPref(PREFS.UI.EDITOR_WIDTH, width);
  };
}

/**
 * Dispatches a SHOW_OBJECT_IN_SIDEBAR action, with a grip property corresponding to the
 * {actor} parameter in the {messageId} message.
 *
 * @param {String} actor: Actor id of the object we want to place in the sidebar.
 * @param {String} messageId: id of the message containing the {actor} parameter.
 */
function showMessageObjectInSidebar(actor, messageId) {
  return ({ dispatch, getState }) => {
    const { parameters } = getMessage(getState(), messageId);
    if (Array.isArray(parameters)) {
      for (const parameter of parameters) {
        if (parameter.actor === actor) {
          dispatch(showObjectInSidebar(parameter));
          return;
        }
      }
    }
  };
}

function showObjectInSidebar(grip) {
  return {
    type: SHOW_OBJECT_IN_SIDEBAR,
    grip,
  };
}

function reverseSearchInputToggle({ initialValue } = {}) {
  return {
    type: REVERSE_SEARCH_INPUT_TOGGLE,
    initialValue,
  };
}

function filterBarDisplayModeSet(displayMode) {
  return {
    type: FILTERBAR_DISPLAY_MODE_SET,
    displayMode,
  };
}

module.exports = {
  contentMessagesToggle,
  editorOnboardingDismiss,
  editorToggle,
  filterBarDisplayModeSet,
  initialize,
  persistToggle,
  reverseSearchInputToggle,
  selectNetworkMessageTab,
  setEditorWidth,
  showMessageObjectInSidebar,
  showObjectInSidebar,
  sidebarClose,
  splitConsoleCloseButtonToggle,
  timestampsToggle,
  warningGroupsToggle,
};
