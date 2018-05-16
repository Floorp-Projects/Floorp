/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPEND_NOTIFICATION,
  REMOVE_NOTIFICATION,
} = require("devtools/client/webconsole/constants");

/**
 * Append a notification into JSTerm notification list.
 */
function appendNotification(label, value, image, priority, buttons = [], eventCallback) {
  return {
    type: APPEND_NOTIFICATION,
    label,
    value,
    image,
    priority,
    buttons,
    eventCallback,
  };
}

/**
 * Remove notification with specified value from JSTerm
 * notification list.
 */
function removeNotification(value) {
  return {
    type: REMOVE_NOTIFICATION,
    value,
  };
}

module.exports = {
  appendNotification,
  removeNotification,
};
