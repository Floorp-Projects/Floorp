/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSource } from "../sources";

describe("sources", () => {
  it("fail getSource", () => {
    const sourceId = "some.nonexistent.source.id";
    expect(() => {
      getSource(sourceId);
    }).toThrow();
  });
});
