/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");

exports.ConsoleCommand = Immutable.Record({
  allowRepeating: false,
  messageText: null,
  source: null,
  type: null,
  category: null,
  severity: null,
});

exports.ConsoleMessage = Immutable.Record({
  allowRepeating: true,
  source: null,
  type: null,
  level: null,
  messageText: null,
  parameters: null,
  repeat: 1,
  repeatId: null,
  category: "output",
  severity: "log",
  id: null,
});
