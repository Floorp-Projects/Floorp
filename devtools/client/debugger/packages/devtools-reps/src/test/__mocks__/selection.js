/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

let selection;
const selectionObject = {
  toString: () => selection,
  get type() {
    if (selection === undefined) {
      return "None";
    }
    if (selection === "") {
      return "Caret";
    }
    return "Range";
  },
  setMockSelection: str => {
    selection = str;
  },
};

global.getSelection = () => selectionObject;
