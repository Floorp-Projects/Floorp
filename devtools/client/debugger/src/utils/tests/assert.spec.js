/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import assert from "../assert.js";

let testAssertMessageHead, testAssertMessage;

describe("assert", () => {
  beforeEach(() => {
    testAssertMessageHead = "Assertion failure: ";
    testAssertMessage = "Test assert.js Message";
  });

  describe("when condition is truthy", () => {
    it("does not throw an Error", () => {
      expect(() => {
        assert(true, testAssertMessage);
      }).not.toThrow();
    });
  });

  describe("when condition is falsy", () => {
    it("throws an Error displaying the passed message", () => {
      expect(() => {
        assert(false, testAssertMessage);
      }).toThrow(new Error(testAssertMessageHead + testAssertMessage));
    });
  });
});
