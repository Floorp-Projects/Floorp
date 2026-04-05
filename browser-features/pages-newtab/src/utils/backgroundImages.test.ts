// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getFloorpImages,
  getRandomBackgroundImage,
  getSelectedFloorpImage,
} from "./backgroundImages.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "getRandomBackgroundImage should return string or null",
    fn: () => {
      const randomImage = getRandomBackgroundImage();
      assert(
        randomImage === null || typeof randomImage === "string",
        "getRandomBackgroundImage should return string or null",
      );
    },
  },
  {
    name: "getFloorpImages should return array with valid entries",
    fn: () => {
      const floorpImages = getFloorpImages();
      assert(
        Array.isArray(floorpImages),
        "getFloorpImages should return array",
      );
      for (const image of floorpImages) {
        assert(
          typeof image.name === "string" && image.name.length > 0,
          "image name should be non-empty",
        );
        assert(
          typeof image.url === "string" && image.url.length > 0,
          "image url should be non-empty",
        );
      }
    },
  },
  {
    name: "selected image should match name lookup",
    fn: () => {
      const floorpImages = getFloorpImages();
      if (floorpImages.length > 0) {
        const first = floorpImages[0];
        const selected = getSelectedFloorpImage(first.name);
        assertEquals(
          selected,
          first.url,
          "selected image should match name lookup",
        );
      }
    },
  },
  {
    name: "null name should return null",
    fn: () =>
      assertEquals(
        getSelectedFloorpImage(null),
        null,
        "null name should return null",
      ),
  },
  {
    name: "unknown image should return null",
    fn: () =>
      assertEquals(
        getSelectedFloorpImage("__missing__"),
        null,
        "unknown image should return null",
      ),
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("backgroundImages.test.ts", tests);
}
