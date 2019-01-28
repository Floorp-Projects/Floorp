/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { truncateMiddleText } from "../text";

describe("text", () => {
  it("should truncate the text in the middle", () => {
    const sourceText = "this is a very long text and ends here";
    expect(truncateMiddleText(sourceText, 30)).toMatch(
      "this is a ver… and ends here"
    );
  });
  it("should keep the text as it is", () => {
    const sourceText = "this is a short text ends here";
    expect(truncateMiddleText(sourceText, 30)).toMatch(
      "this is a short text ends here"
    );
  });
});
