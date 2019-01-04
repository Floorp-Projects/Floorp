/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { hasSource, getSource, setSource, clearSources } from "../sources";

import type { Source } from "../../../types";

describe("sources", () => {
  it("hasSource", () => {
    const sourceId = "some.source.id";
    const source: Source = {
      id: sourceId,
      url: "http://example.org/some.source.js",
      isBlackBoxed: false,
      isPrettyPrinted: false,
      isWasm: false,
      loadedState: "loaded"
    };

    expect(hasSource(sourceId)).toEqual(false);
    setSource(source);
    expect(hasSource(sourceId)).toEqual(true);
    expect(getSource(sourceId)).toEqual(source);
    clearSources();
    expect(hasSource(sourceId)).toEqual(false);
  });

  it("fail getSource", () => {
    const sourceId = "some.nonexistent.source.id";
    expect(hasSource(sourceId)).toEqual(false);
    expect(() => {
      getSource(sourceId);
    }).toThrow();
  });
});
