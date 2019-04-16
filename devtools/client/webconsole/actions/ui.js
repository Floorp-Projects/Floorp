/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
} = require("devtools/client/webconsole/constants");

function persistToggle() {
  return ({dispatch, getState, prefsService}) => {
    dispatch({
      type: PERSIST_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.PERSIST, uiState.persistLogs);
  };
}

function contentMessagesToggle() {
  return ({dispatch, getState, prefsService}) => {
    dispatch({
      type: SHOW_CONTENT_MESSAGES_TOGGLE,
    });
    const uiState = getAllUi(getState());
    prefsService.setBoolPref(PREFS.UI.CONTENT_MESSAGES, uiState.showContentMessages);
  };
}

function timestampsToggle(visible) {
  return {
    type: TIMESTAMPS_TOGGLE,
    visible,
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

function sidebarClose(show) {
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

/**
 * Dispatches a SHOW_OBJECT_IN_SIDEBAR action, with a grip property corresponding to the
 * {actor} parameter in the {messageId} message.
 *
 * @param {String} actor: Actor id of the object we want to place in the sidebar.
 * @param {String} messageId: id of the message containing the {actor} parameter.
 */
function showMessageObjectInSidebar(actor, messageId) {
  return ({dispatch, getState}) => {
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

function reverseSearchInputToggle({initialValue} = {}) {
  return {
    type: REVERSE_SEARCH_INPUT_TOGGLE,
    initialValue,
  };
}

module.exports = {
  contentMessagesToggle,
  initialize,
  persistToggle,
  reverseSearchInputToggle,
  selectNetworkMessageTab,
  showMessageObjectInSidebar,
  showObjectInSidebar,
  sidebarClose,
  splitConsoleCloseButtonToggle,
  timestampsToggle,
};
