/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {DebuggerClient} = require("./debugger-client");
const EnvironmentClient = require("./environment-client");
const LongStringClient = require("./long-string-client");
const ObjectClient = require("./object-client");
const RootClient = require("./root-client");

module.exports = {
  DebuggerClient,
  EnvironmentClient,
  LongStringClient,
  ObjectClient,
  RootClient,
};
