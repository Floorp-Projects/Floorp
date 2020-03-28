/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllPrefs } = require("devtools/client/webconsole/selectors/prefs");
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
  EAGER_EVALUATION_TOGGLE,
  AUTOCOMPLETE_TOGGLE,
} = require("devtools/client/webconsole/constants");

function openLink(url, e) {
  return ({ hud }) => {
    return hud.openLink(url, e);
  };
}

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

function timestampsToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: TIMESTAMPS_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(
      PREFS.UI.MESSAGE_TIMESTAMP,
      uiState.timestampsVisible
    );
  };
}

function autocompleteToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: AUTOCOMPLETE_TOGGLE,
    });
    const prefsState = getAllPrefs(getState());
    prefsService.setBoolPref(
      PREFS.FEATURES.AUTOCOMPLETE,
      prefsState.autocomplete
    );
  };
}

function warningGroupsToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: WARNING_GROUPS_TOGGLE,
    });
    const prefsState = getAllPrefs(getState());
    prefsService.setBoolPref(
      PREFS.FEATURES.GROUP_WARNINGS,
      prefsState.groupWarnings
    );
  };
}

function eagerEvaluationToggle() {
  return ({ dispatch, getState, prefsService }) => {
    dispatch({
      type: EAGER_EVALUATION_TOGGLE,
    });
    const prefsState = getAllPrefs(getState());
    prefsService.setBoolPref(
      PREFS.FEATURES.EAGER_EVALUATION,
      prefsState.eagerEvaluation
    );
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
 * @param {String} actorID: Actor id of the object we want to place in the sidebar.
 * @param {String} messageId: id of the message containing the {actor} parameter.
 */
function showMessageObjectInSidebar(actorID, messageId) {
  return ({ dispatch, getState }) => {
    const { parameters } = getMessage(getState(), messageId);
    if (Array.isArray(parameters)) {
      for (const parameter of parameters) {
        if (parameter && parameter.actorID === actorID) {
          dispatch(showObjectInSidebar(parameter));
          return;
        }
      }
    }
  };
}

function showObjectInSidebar(front) {
  return {
    type: SHOW_OBJECT_IN_SIDEBAR,
    front,
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

function openSidebar(messageId, rootActorId) {
  return ({ dispatch }) => {
    dispatch(showMessageObjectInSidebar(rootActorId, messageId));
  };
}

module.exports = {
  contentMessagesToggle,
  eagerEvaluationToggle,
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
  openLink,
  openSidebar,
  autocompleteToggle,
};
