// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getFloorpImages,
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "./backgroundImages.ts";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals(actual: unknown, expected: unknown, message: string): void {
  if (!Object.is(actual, expected)) {
    throw new Error(`${message}: expected=${String(expected)} actual=${String(actual)}`);
  }
}

export function runAllTests(): void {
  const randomImage = getRandomBackgroundImage();
  assert(
    randomImage === null || typeof randomImage === "string",
    "getRandomBackgroundImage should return string or null",
  );

  const floorpImages = getFloorpImages();
  assert(Array.isArray(floorpImages), "getFloorpImages should return array");

  for (const image of floorpImages) {
    assert(typeof image.name === "string" && image.name.length > 0, "image name should be non-empty");
    assert(typeof image.url === "string" && image.url.length > 0, "image url should be non-empty");
  }

  if (floorpImages.length > 0) {
    const first = floorpImages[0];
    const selected = getSelectedFloorpImage(first.name);
    assertEquals(selected, first.url, "selected image should match name lookup");
  }

  assertEquals(getSelectedFloorpImage(null), null, "null name should return null");
  assertEquals(getSelectedFloorpImage("__missing__"), null, "unknown image should return null");
}
