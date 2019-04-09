/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { parseScript } from "./utils/ast";

export function hasSyntaxError(input: string): string | false {
  try {
    parseScript(input);
    return false;
  } catch (e) {
    return `${e.name} : ${e.message}`;
  }
}
