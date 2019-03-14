/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { findEmptyLines } from "../empty-lines";
import { makeSource } from "../test-head";

function ml(gLine) {
  const generatedLocation = { line: gLine, column: 0, sourceId: "foo" };
  return { generatedLocation, location: generatedLocation };
}

describe("emptyLines", () => {
  it("no positions", () => {
    const source = makeSource("foo", { text: "\n" });
    const lines = findEmptyLines(source, []);
    expect(lines).toEqual([1, 2]);
  });

  it("one position", () => {
    const source = makeSource("foo", { text: "\n" });
    const lines = findEmptyLines(source, [ml(1)]);
    expect(lines).toEqual([2]);
  });

  it("outside positions are not included", () => {
    const source = makeSource("foo", { text: "\n" });
    const lines = findEmptyLines(source, [ml(10)]);
    expect(lines).toEqual([1, 2]);
  });
});
