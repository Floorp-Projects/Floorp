/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isMinified } from "../isMinified";
import { makeMockSourceWithContent } from "../test-mockup";

describe("isMinified", () => {
  it("no indents", () => {
    const sourceWithContent = makeMockSourceWithContent(
      undefined,
      undefined,
      undefined,
      "function base(boo) {\n}"
    );
    expect(isMinified(sourceWithContent)).toBe(true);
  });
});
