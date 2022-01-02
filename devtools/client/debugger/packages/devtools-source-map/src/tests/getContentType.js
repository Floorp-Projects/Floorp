/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { getContentType, contentMapForTesting } = require("../utils");

describe("getContentType", () => {
  for (const ext in contentMapForTesting) {
    test(`extension - ${ext}`, () => {
      expect(getContentType(`whatever.${ext}`)).toEqual(
        contentMapForTesting[ext]
      );
      expect(getContentType(`whatever${ext}`)).toEqual("text/plain");
    });
  }

  test("bad extension", () => {
    expect(getContentType("whatever.platypus")).toEqual("text/plain");
  });
});
