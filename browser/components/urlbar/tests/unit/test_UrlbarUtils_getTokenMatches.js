/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests UrlbarUtils.getTokenMatches.
 */

"use strict";

add_task(function test() {
  const tests = [
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [0, 7],
        [8, 2],
      ],
    },
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "MOZILLA IS for the Open Web",
      expected: [
        [0, 7],
        [8, 2],
      ],
    },
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "MoZiLlA Is for the Open Web",
      expected: [
        [0, 7],
        [8, 2],
      ],
    },
    {
      tokens: ["MOZILLA", "IS", "I"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [0, 7],
        [8, 2],
      ],
    },
    {
      tokens: ["MoZiLlA", "Is", "I"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [0, 7],
        [8, 2],
      ],
    },
    {
      tokens: ["mo", "b"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [0, 2],
        [26, 1],
      ],
    },
    {
      tokens: ["mo", "b"],
      phrase: "MOZILLA is for the OPEN WEB",
      expected: [
        [0, 2],
        [26, 1],
      ],
    },
    {
      tokens: ["MO", "B"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [0, 2],
        [26, 1],
      ],
    },
    {
      tokens: ["mo", ""],
      phrase: "mozilla is for the Open Web",
      expected: [[0, 2]],
    },
    {
      tokens: ["mozilla"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["mozilla"],
      phrase: "MOZILLA",
      expected: [[0, 7]],
    },
    {
      tokens: ["mozilla"],
      phrase: "MoZiLlA",
      expected: [[0, 7]],
    },
    {
      tokens: ["mozilla"],
      phrase: "mOzIlLa",
      expected: [[0, 7]],
    },
    {
      tokens: ["MOZILLA"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["MoZiLlA"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["mOzIlLa"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["\u9996"],
      phrase: "Test \u9996\u9875 Test",
      expected: [[5, 1]],
    },
    {
      tokens: ["mo", "zilla"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["mo", "zilla"],
      phrase: "MOZILLA",
      expected: [[0, 7]],
    },
    {
      tokens: ["mo", "zilla"],
      phrase: "MoZiLlA",
      expected: [[0, 7]],
    },
    {
      tokens: ["mo", "zilla"],
      phrase: "mOzIlLa",
      expected: [[0, 7]],
    },
    {
      tokens: ["MO", "ZILLA"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["Mo", "Zilla"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["moz", "zilla"],
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: [""], // Should never happen in practice.
      phrase: "mozilla",
      expected: [],
    },
    {
      tokens: ["mo", "om"],
      phrase: "mozilla mozzarella momo",
      expected: [
        [0, 2],
        [8, 2],
        [19, 4],
      ],
    },
    {
      tokens: ["mo", "om"],
      phrase: "MOZILLA MOZZARELLA MOMO",
      expected: [
        [0, 2],
        [8, 2],
        [19, 4],
      ],
    },
    {
      tokens: ["MO", "OM"],
      phrase: "mozilla mozzarella momo",
      expected: [
        [0, 2],
        [8, 2],
        [19, 4],
      ],
    },
    {
      tokens: ["resume"],
      phrase: "résumé",
      expected: [[0, 6]],
    },
    {
      // This test should succeed even in a Spanish locale where N and Ñ are
      // considered distinct letters.
      tokens: ["jalapeno"],
      phrase: "jalapeño",
      expected: [[0, 8]],
    },
  ];
  for (let { tokens, phrase, expected } of tests) {
    tokens = tokens.map(t => ({
      value: t,
      lowerCaseValue: t.toLocaleLowerCase(),
    }));
    Assert.deepEqual(
      UrlbarUtils.getTokenMatches(tokens, phrase, UrlbarUtils.HIGHLIGHT.TYPED),
      expected,
      `Match "${tokens.map(t => t.value).join(", ")}" on "${phrase}"`
    );
  }
});

/**
 * Tests suggestion highlighting. Note that suggestions are only highlighted if
 * the matching token is at the beginning of a word in the matched string.
 */
add_task(function testSuggestions() {
  const tests = [
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "mozilla is for the Open Web",
      expected: [
        [7, 1],
        [10, 17],
      ],
    },
    {
      tokens: ["\u9996"],
      phrase: "Test \u9996\u9875 Test",
      expected: [
        [0, 5],
        [6, 6],
      ],
    },
    {
      tokens: ["mo", "zilla"],
      phrase: "mOzIlLa",
      expected: [[2, 5]],
    },
    {
      tokens: ["MO", "ZILLA"],
      phrase: "mozilla",
      expected: [[2, 5]],
    },
    {
      tokens: [""], // Should never happen in practice.
      phrase: "mozilla",
      expected: [[0, 7]],
    },
    {
      tokens: ["mo", "om", "la"],
      phrase: "mozilla mozzarella momo",
      expected: [
        [2, 6],
        [10, 9],
        [21, 2],
      ],
    },
    {
      tokens: ["mo", "om", "la"],
      phrase: "MOZILLA MOZZARELLA MOMO",
      expected: [
        [2, 6],
        [10, 9],
        [21, 2],
      ],
    },
    {
      tokens: ["MO", "OM", "LA"],
      phrase: "mozilla mozzarella momo",
      expected: [
        [2, 6],
        [10, 9],
        [21, 2],
      ],
    },
  ];
  for (let { tokens, phrase, expected } of tests) {
    tokens = tokens.map(t => ({
      value: t,
      lowerCaseValue: t.toLocaleLowerCase(),
    }));
    Assert.deepEqual(
      UrlbarUtils.getTokenMatches(
        tokens,
        phrase,
        UrlbarUtils.HIGHLIGHT.SUGGESTED
      ),
      expected,
      `Match "${tokens.map(t => t.value).join(", ")}" on "${phrase}"`
    );
  }
});
