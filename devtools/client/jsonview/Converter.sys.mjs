/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Create instances of the JSON view converter.
 */
export function Converter() {
  const { require } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/Loader.sys.mjs"
  );
  const {
    JsonViewService,
  } = require("resource://devtools/client/jsonview/converter-child.js");

  return JsonViewService.createInstance();
}
