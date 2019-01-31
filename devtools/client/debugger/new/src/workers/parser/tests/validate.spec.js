/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { hasSyntaxError } from "../validate";

describe("has syntax error", () => {
  it("should return false", () => {
    expect(hasSyntaxError("foo")).toEqual(false);
  });

  it("should return the error object for the invalid expression", () => {
    expect(hasSyntaxError("foo)(")).toMatchSnapshot();
  });
});
