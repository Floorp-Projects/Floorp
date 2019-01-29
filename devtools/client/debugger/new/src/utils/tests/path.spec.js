/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { basename, dirname, isURL, isAbsolute, join } from "../path";

const fullTestURL = "https://www.example.com/some/endpoint";
const absoluteTestPath = "/some/absolute/path/to/resource";
const aTestName = "name";

describe("basename()", () => {
  it("returns the basename of the path", () => {
    expect(basename(fullTestURL)).toBe("endpoint");
  });
});

describe("dirname()", () => {
  it("returns the current directory in a path", () => {
    expect(dirname(fullTestURL)).toBe("https://www.example.com/some");
  });
});

describe("isURL()", () => {
  it("returns true if a string contains characters denoting a scheme", () => {
    expect(isURL(fullTestURL)).toBe(true);
  });

  it("returns false if string does not denote a scheme", () => {
    expect(isURL(absoluteTestPath)).toBe(false);
  });
});

describe("isAbsolute()", () => {
  it("returns true if a string begins with a slash", () => {
    expect(isAbsolute(absoluteTestPath)).toBe(true);
  });

  it("returns false if a string does not begin with a slash", () => {
    expect(isAbsolute(fullTestURL)).toBe(false);
  });
});

describe("join()", () => {
  it("concatenates a base path and a directory name", () => {
    expect(join(absoluteTestPath, aTestName)).toBe(
      "/some/absolute/path/to/resource/name"
    );
  });
});
