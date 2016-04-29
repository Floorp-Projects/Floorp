/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/webconsole/new-console-output/constants");

function messages(state = [], action) {
  switch (action.type) {
    case constants.MESSAGE_ADD:
      let newMessage = action.message;
      if (newMessage.allowRepeating && state.length > 0) {
        let lastMessage = state[state.length - 1];
        if (lastMessage.repeatId === newMessage.repeatId) {
          newMessage.repeat = lastMessage.repeat + 1;
          return state.slice(0, state.length - 1).concat(newMessage);
        }
      }
      return state.concat([ newMessage ]);
    case constants.MESSAGES_CLEAR:
      return [];
  }

  return state;
}

exports.messages = messages;
