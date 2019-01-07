/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { highlightMatches } from "../project-search";

describe("project search - highlightMatches", () => {
  it("simple", () => {
    const lineMatch = {
      value: "This is a sample sentence",
      column: 17,
      matchIndex: 17,
      match: "sentence"
    };
    expect(highlightMatches(lineMatch)).toMatchSnapshot();
  });
});
