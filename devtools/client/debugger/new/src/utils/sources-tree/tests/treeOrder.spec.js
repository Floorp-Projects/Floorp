/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getDomain } from "../treeOrder";

describe("getDomain", () => {
  it("parses a url and returns the host name", () => {
    expect(getDomain("http://www.mozilla.com")).toBe("mozilla.com");
  });

  it("returns null for an undefined string", () => {
    expect(getDomain(undefined)).toBe(null);
  });

  it("returns null for an empty string", () => {
    expect(getDomain("")).toBe(null);
  });

  it("returns null for a poorly formed string", () => {
    expect(getDomain("\\/~`?,.{}[]!@$%^&*")).toBe(null);
  });
});
