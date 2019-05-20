/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function documentHasSelection(): boolean {
  const selection = getSelection();
  if (!selection) {
    return false;
  }

  return selection.type === "Range";
}

module.exports = {
  documentHasSelection,
};
