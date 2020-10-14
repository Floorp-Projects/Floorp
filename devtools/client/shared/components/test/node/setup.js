/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* global global */

"use strict";

// Configure enzyme with React 16 adapter.
const Enzyme = require("enzyme");
const Adapter = require("enzyme-adapter-react-16");
Enzyme.configure({ adapter: new Adapter() });

global.requestAnimationFrame = function(cb) {
  cb();
  return null;
};

// Mock getSelection

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

// Array#flatMap is only supported in Node 11+
if (!Array.prototype.flatMap) {
  // eslint-disable-next-line no-extend-native
  Array.prototype.flatMap = function(cb) {
    return this.reduce((acc, x, i, arr) => {
      return acc.concat(cb(x, i, arr));
    }, []);
  };
}
