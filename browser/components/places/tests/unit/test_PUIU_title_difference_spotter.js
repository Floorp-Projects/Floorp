/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the difference indication for titles.
 */

const TESTS = [
  {
    title: null,
    expected: undefined,
  },
  {
    title: "Short title",
    expected: undefined,
  },
  {
    title: "Short title2",
    expected: undefined,
  },
  {
    title: "Long title same as another",
    expected: undefined,
  },
  {
    title: "Long title same as another",
    expected: undefined,
  },
  {
    title: "Long title with difference at the end 1",
    expected: 38,
  },
  {
    title: "Long title with difference at the end 2",
    expected: 38,
  },
  {
    title: "A long title with difference 123456 in the middle.",
    expected: 30,
  },
  {
    title: "A long title with difference 135246 in the middle.",
    expected: 30,
  },
  {
    title:
      "Some long titles with variable 12345678 differences to 13572468 other titles",
    expected: 32,
  },
  {
    title:
      "Some long titles with variable 12345678 differences to 15263748 other titles",
    expected: 32,
  },
  {
    title:
      "Some long titles with variable 15263748 differences to 12345678 other titles",
    expected: 32,
  },
  {
    title: "One long title which will be shorter than the other one",
    expected: 40,
  },
  {
    title:
      "One long title which will be shorter that the other one (not this one)",
    expected: 40,
  },
];

add_task(async function test_difference_finding() {
  PlacesUIUtils.insertTitleStartDiffs(TESTS);

  for (let result of TESTS) {
    Assert.equal(
      result.titleDifferentIndex,
      result.expected,
      `Should have returned the correct index for "${result.title}"`
    );
  }
});
