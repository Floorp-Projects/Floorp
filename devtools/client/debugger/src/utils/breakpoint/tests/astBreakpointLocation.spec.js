/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getASTLocation } from "../astBreakpointLocation.js";
import {
  populateSource,
  populateOriginalSource,
} from "../../../workers/parser/tests/helpers";
import { getSymbols } from "../../../workers/parser/getSymbols";
import cases from "jest-in-case";

async function setup({ file, location, functionName, original }) {
  const source = original ? populateOriginalSource(file) : populateSource(file);

  const symbols = getSymbols(source.id);

  const astLocation = getASTLocation(source, symbols, location);
  expect(astLocation.name).toBe(functionName);
  expect(astLocation).toMatchSnapshot();
}

describe("ast", () => {
  cases("valid location", setup, [
    {
      name: "returns the scope and offset",
      file: "math",
      location: { line: 6, column: 0 },
      functionName: "math",
    },
    {
      name: "returns name for a nested anon fn as the parent func",
      file: "outOfScope",
      location: { line: 25, column: 0 },
      functionName: "outer",
    },
    {
      name: "returns name for a nested named fn",
      file: "outOfScope",
      location: { line: 5, column: 0 },
      functionName: "inner",
    },
    {
      name: "returns name for an anon fn with a named variable",
      file: "outOfScope",
      location: { line: 40, column: 0 },
      functionName: "globalDeclaration",
    },
  ]);

  cases("invalid location", setup, [
    {
      name: "returns the scope name for global scope as undefined",
      file: "class",
      original: true,
      location: { line: 10, column: 0 },
      functionName: undefined,
    },
    {
      name: "returns name for an anon fn in global scope as undefined",
      file: "outOfScope",
      location: { line: 44, column: 0 },
      functionName: undefined,
    },
  ]);
});
