/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { MODE } = require("../constants");
const stubs = require("../stubs/grip-array");
const { getGripLengthBubbleText } = require("./test-helpers");

describe("getGripLengthBubbleText - Zero length", () => {
  const object = stubs.get("testBasic");

  it("length bubble is invisible", () => {
    const output = "";
    let text = getGripLengthBubbleText(object, { mode: undefined });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.SHORT });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.LONG });
    expect(text).toBe(output);
  });

  it("length bubble is visible", () => {
    const output = "(0)";
    let text = getGripLengthBubbleText(object, {
      mode: undefined,
      showZeroLength: true,
    });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, {
      mode: MODE.TINY,
      showZeroLength: true,
    });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, {
      mode: MODE.SHORT,
      showZeroLength: true,
    });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, {
      mode: MODE.LONG,
      showZeroLength: true,
    });
    expect(text).toBe(output);
  });
});

describe("getGripLengthBubbleText - Obvious length for some modes", () => {
  const object = stubs.get("testMoreThanShortMaxProps");
  const visibleOutput = `(${object.preview.length})`;

  it("text renders as expected", () => {
    let text = getGripLengthBubbleText(object, { mode: undefined });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, { mode: MODE.SHORT });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, { mode: MODE.LONG });
    expect(text).toBe(visibleOutput);

    const visibilityThreshold = 5;
    text = getGripLengthBubbleText(object, {
      mode: undefined,
      visibilityThreshold,
    });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, {
      mode: MODE.TINY,
      visibilityThreshold,
    });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, {
      mode: MODE.SHORT,
      visibilityThreshold,
    });
    expect(text).toBe(visibleOutput);

    text = getGripLengthBubbleText(object, {
      mode: MODE.LONG,
      visibilityThreshold,
    });
    expect(text).toBe("");
  });
});

describe("getGripLengthBubbleText - Visible length", () => {
  const object = stubs.get("testMoreThanLongMaxProps");
  const output = `(${object.preview.length})`;

  it("length bubble is always visible", () => {
    let text = getGripLengthBubbleText(object, { mode: undefined });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.TINY });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.SHORT });
    expect(text).toBe(output);

    text = getGripLengthBubbleText(object, { mode: MODE.LONG });
    expect(text).toBe(output);
  });
});
