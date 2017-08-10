/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { receive } = require("../event/utils");
const { InputPort } = require("./system");
const { object} = require("../util/sequence");
const { getOuterId } = require("../window/utils");

const Input = function() {};
Input.prototype = Object.create(InputPort.prototype);

Input.prototype.onCustomizeStart = function (window) {
  receive(this, object([getOuterId(window), true]));
}

Input.prototype.onCustomizeEnd = function (window) {
  receive(this, object([getOuterId(window), null]));
}

Input.prototype.addListener = input => CustomizableUI.addListener(input);

Input.prototype.removeListener = input => CustomizableUI.removeListener(input);

exports.CustomizationInput = Input;
