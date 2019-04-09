/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

let logs = [];
export function getHistory(query: ?string = null): any {
  if (!query) {
    return logs;
  }

  return logs.filter(log => log.type === query);
}

export function clearHistory() {
  logs = [];
}
