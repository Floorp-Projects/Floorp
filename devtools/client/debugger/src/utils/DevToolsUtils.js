/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import assert from "./assert";

export function reportException(who, exception) {
  const msg = `${who} threw an exception: `;
  console.error(msg, exception);
}

export function executeSoon(fn) {
  setTimeout(fn, 0);
}

export default assert;
