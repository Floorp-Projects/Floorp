/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { findSourceMatches } from "../project-search";

const text = `
  function foo() {
    foo();
  }
`;

describe("project search", () => {
  const content = {
    type: "text",
    value: text,
    contentType: undefined,
  };

  it("finds matches", () => {
    const matches = findSourceMatches("bar.js", content, "foo");
    expect(matches).toEqual([
      {
        column: 11,
        line: 2,
        match: "foo",
        matchIndex: 11,
        sourceId: "bar.js",
        value: "  function foo() {",
      },
      {
        column: 4,
        line: 3,
        match: "foo",
        matchIndex: 4,
        sourceId: "bar.js",
        value: "    foo();",
      },
    ]);
  });

  it("finds no matches in source", () => {
    const matches = findSourceMatches("bar.js", content, "test");
    expect(matches).toEqual([]);
  });
});
