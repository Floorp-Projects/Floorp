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
      expected: [[0, 7], [8, 2]],
    },
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "MOZILLA IS for the Open Web",
      expected: [[0, 7], [8, 2]],
    },
    {
      tokens: ["mozilla", "is", "i"],
      phrase: "MoZiLlA Is for the Open Web",
      expected: [[0, 7], [8, 2]],
    },
    {
      tokens: ["MOZILLA", "IS", "I"],
      phrase: "mozilla is for the Open Web",
      expected: [[0, 7], [8, 2]],
    },
    {
      tokens: ["MoZiLlA", "Is", "I"],
      phrase: "mozilla is for the Open Web",
      expected: [[0, 7], [8, 2]],
    },
    {
      tokens: ["mo", "b"],
      phrase: "mozilla is for the Open Web",
      expected: [[0, 2], [26, 1]],
    },
    {
      tokens: ["mo", "b"],
      phrase: "MOZILLA is for the OPEN WEB",
      expected: [[0, 2], [26, 1]],
    },
    {
      tokens: ["MO", "B"],
      phrase: "mozilla is for the Open Web",
      expected: [[0, 2], [26, 1]],
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
      expected: [[0, 2], [8, 2], [19, 4]],
    },
    {
      tokens: ["mo", "om"],
      phrase: "MOZILLA MOZZARELLA MOMO",
      expected: [[0, 2], [8, 2], [19, 4]],
    },
    {
      tokens: ["MO", "OM"],
      phrase: "mozilla mozzarella momo",
      expected: [[0, 2], [8, 2], [19, 4]],
    },
  ];
  for (let {tokens, phrase, expected} of tests) {
    tokens = tokens.map(t => ({
      value: t,
      lowerCaseValue: t.toLocaleLowerCase(),
    }));
    Assert.deepEqual(UrlbarUtils.getTokenMatches(tokens, phrase), expected,
                     `Match "${tokens.map(t => t.value).join(", ")}" on "${phrase}"`);
  }
});
