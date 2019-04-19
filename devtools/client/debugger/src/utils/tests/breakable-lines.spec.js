/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { findBreakableLines } from "../breakable-lines";
import { createSourceObject } from "../test-head";

function ml(gLine) {
  const generatedLocation = { line: gLine, column: 0, sourceId: "foo" };
  return { generatedLocation, location: generatedLocation };
}

describe("breakableLines", () => {
  it("no positions", () => {
    const source = createSourceObject("foo");
    const lines = findBreakableLines(source, []);
    expect(lines).toEqual([]);
  });

  it("one position", () => {
    const source = createSourceObject("foo");
    const lines = findBreakableLines(source, [ml(1)]);
    expect(lines).toEqual([1]);
  });

  it("outside positions are not included", () => {
    const source = createSourceObject("foo");
    const lines = findBreakableLines(source, [ml(1), ml(2), ml(10)]);
    expect(lines).toEqual([1, 2, 10]);
  });
});
