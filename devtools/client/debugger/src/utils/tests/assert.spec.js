/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import assert from "../assert.js";

let testAssertMessageHead, testAssertMessage;

describe("assert", () => {
  beforeEach(() => {
    testAssertMessageHead = "Assertion failure: ";
    testAssertMessage = "Test assert.js Message";
  });

  describe("when isDevelopment and the condition is truthy", () => {
    it("does not throw an Error", () => {
      expect(() => {
        assert(true, testAssertMessage);
      }).not.toThrow();
    });
  });

  describe("when isDevelopment and the condition is falsy", () => {
    it("throws an Error displaying the passed message", () => {
      expect(() => {
        assert(false, testAssertMessage);
      }).toThrow(new Error(testAssertMessageHead + testAssertMessage));
    });
  });

  describe("when not isDevelopment", () => {
    it("does not throw an Error", () => {
      process.env.NODE_ENV = "production";
      expect(() => assert(false, testAssertMessage)).not.toThrow();
      delete process.env.NODE_ENV;
    });
  });
});
