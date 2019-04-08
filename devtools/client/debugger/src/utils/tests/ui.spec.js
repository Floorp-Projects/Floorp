/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isVisible } from "../ui";

describe("ui", () => {
  it("should return #mount width", () => {
    if (!document.body) {
      throw new Error("no document body");
    }
    document.body.innerHTML = "<div id='mount'></div>";
    expect(isVisible()).toBeGreaterThanOrEqual(0);
  });
});
