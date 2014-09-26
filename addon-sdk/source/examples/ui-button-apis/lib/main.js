/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var data = require('sdk/self').data;
var tabs = require('sdk/tabs');
var { notify } = require('sdk/notifications');
var { ActionButton, ToggleButton } = require('sdk/ui');

var icon = 'chrome://mozapps/skin/extensions/extensionGeneric.png';
exports.icon = icon;

// your basic action button
var action = ActionButton({
  id: 'test-action-button',
  label: 'Action Button',
  icon: icon,
  onClick: function (state) {
    notify({
      title: "Action!",
      text: "This notification was triggered from an action button!",
    });
  }
});
exports.actionButton = action;

var toggle = ToggleButton({
  id: 'test-toggle-button',
  label: 'Toggle Button',
  icon: icon,
  onClick: function (state) {
    notify({
      title: "Toggled!",
      text: "The current state of the button is " + state.checked,
    });
  }
});
exports.toggleButton = toggle;
