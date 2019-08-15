/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { findBestMatchExpression } from "../ast";

import { getSymbols } from "../../workers/parser/getSymbols";
import { populateSource } from "../../workers/parser/tests/helpers";

describe("find the best expression for the token", () => {
  const source = populateSource("computed-props");
  const symbols = getSymbols(source.id);

  it("should find the identifier", () => {
    const expression = findBestMatchExpression(symbols, {
      line: 1,
      column: 13,
    });
    expect(expression).toMatchSnapshot();
  });

  it("should find the expression for the property", () => {
    const expression = findBestMatchExpression(symbols, {
      line: 6,
      column: 16,
    });
    expect(expression).toMatchSnapshot();
  });

  it("should find the identifier for computed member expressions", () => {
    const expression = findBestMatchExpression(symbols, { line: 5, column: 6 });
    expect(expression).toMatchSnapshot();
  });
});
