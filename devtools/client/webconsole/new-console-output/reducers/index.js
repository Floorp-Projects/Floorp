/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { filters } = require("./filters");
const { messages } = require("./messages");
const { prefs } = require("./prefs");
const { ui } = require("./ui");

exports.reducers = {
  filters,
  messages,
  prefs,
  ui,
};
