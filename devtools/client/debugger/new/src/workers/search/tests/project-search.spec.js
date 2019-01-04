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
  const emptyResults = [];

  it("throws on lack of source", () => {
    const needle = "test";
    const source = null;
    const matches = () => findSourceMatches(source, needle);
    expect(matches).toThrow(TypeError);
  });

  it("handles empty source object", () => {
    const needle = "test";
    const source = {};
    const matches = findSourceMatches(source, needle);
    expect(matches).toEqual(emptyResults);
  });

  it("finds matches", () => {
    const needle = "foo";
    const source = {
      text,
      loadedState: "loaded",
      id: "bar.js",
      url: "http://example.com/foo/bar.js"
    };

    const matches = findSourceMatches(source, needle);
    expect(matches).toMatchSnapshot();
  });

  it("finds no matches in source", () => {
    const needle = "test";
    const source = {
      text,
      loadedState: "loaded",
      id: "bar.js",
      url: "http://example.com/foo/bar.js"
    };
    const matches = findSourceMatches(source, needle);
    expect(matches).toEqual(emptyResults);
  });
});
