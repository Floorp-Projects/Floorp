/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { findSourceMatches } from "../project-search";

const text = `
  function foo() {
    foo();
  }
`;

describe("project search", () => {
  const emptyResults = [];

  it("finds matches", () => {
    const needle = "foo";
    const content = {
      type: "text",
      value: text,
      contentType: undefined,
    };

    const matches = findSourceMatches("bar.js", content, needle);
    expect(matches).toMatchSnapshot();
  });

  it("finds no matches in source", () => {
    const needle = "test";
    const content = {
      type: "text",
      value: text,
      contentType: undefined,
    };
    const matches = findSourceMatches("bar.js", content, needle);
    expect(matches).toEqual(emptyResults);
  });
});
