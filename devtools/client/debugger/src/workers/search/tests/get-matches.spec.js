/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import getMatches from "../get-matches";

describe("search", () => {
  describe("getMatches", () => {
    it("gets basic string match", () => {
      const text = "the test string with test in it multiple times test.";
      const query = "test";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      });
      expect(matchLocations).toHaveLength(3);
    });

    it("gets basic string match case-sensitive", () => {
      const text = "the Test string with test in it multiple times test.";
      const query = "Test";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      });
      expect(matchLocations).toHaveLength(1);
    });

    it("gets whole word string match", () => {
      const text = "the test string test in it multiple times whoatestthe.";
      const query = "test";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: false,
      });
      expect(matchLocations).toHaveLength(2);
    });

    it("gets regex match", () => {
      const text = "the test string test in it multiple times whoatestthe.";
      const query = "(\\w+)\\s+(\\w+)";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: true,
      });
      expect(matchLocations).toHaveLength(4);
    });

    it("it doesnt fail on empty data", () => {
      const text = "";
      const query = "";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: true,
      });
      expect(matchLocations).toHaveLength(0);
    });

    it("fails gracefully when the line is too long", () => {
      const text = Array(100002).join("x");
      const query = "query";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: true,
      });
      expect(matchLocations).toHaveLength(0);
    });

    // regression test for #6896
    it("doesn't crash on the regex 'a*'", () => {
      const text = "abc";
      const query = "a*";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: true,
      });
      expect(matchLocations).toHaveLength(4);
    });

    // regression test for #6896
    it("doesn't crash on the regex '^'", () => {
      const text = "012";
      const query = "^";
      const matchLocations = getMatches(query, text, {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: true,
      });
      expect(matchLocations).toHaveLength(1);
    });
  });
});
