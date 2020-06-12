/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["Converter"];

/*
 * Create instances of the JSON view converter.
 */
function Converter() {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );
  const {
    JsonViewService,
  } = require("devtools/client/jsonview/converter-child");

  return JsonViewService.createInstance();
}
