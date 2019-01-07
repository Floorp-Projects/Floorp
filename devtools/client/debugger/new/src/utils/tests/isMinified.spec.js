/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isMinified } from "../isMinified";

describe("isMinified", () => {
  it("no indents", () => {
    const source = { id: "no-indents", text: "function base(boo) {\n}" };
    expect(isMinified(source)).toBe(true);
  });
});
