// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

type BackgroundImagesModule =
  typeof import("../../src/utils/backgroundImages.ts");

let backgroundImagesModule: Promise<BackgroundImagesModule> | null = null;

function loadBackgroundImagesModule(): Promise<BackgroundImagesModule> {
  backgroundImagesModule ??= import("../../src/utils/backgroundImages.ts");
  return backgroundImagesModule;
}

const tests: TestCase[] = [
  {
    name: "getRandomBackgroundImage should return string or null",
    fn: async () => {
      const { getRandomBackgroundImage } = await loadBackgroundImagesModule();
      const randomImage = getRandomBackgroundImage();
      assert(
        randomImage === null || typeof randomImage === "string",
        "getRandomBackgroundImage should return string or null",
      );
    },
  },
  {
    name: "getFloorpImages should return array with valid entries",
    fn: async () => {
      const { getFloorpImages } = await loadBackgroundImagesModule();
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
    fn: async () => {
      const { getFloorpImages, getSelectedFloorpImage } =
        await loadBackgroundImagesModule();
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
    fn: async () => {
      const { getSelectedFloorpImage } = await loadBackgroundImagesModule();
      assertEquals(
        getSelectedFloorpImage(null),
        null,
        "null name should return null",
      );
    },
  },
  {
    name: "unknown image should return null",
    fn: async () => {
      const { getSelectedFloorpImage } = await loadBackgroundImagesModule();
      assertEquals(
        getSelectedFloorpImage("__missing__"),
        null,
        "unknown image should return null",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("backgroundImages.test.ts", tests);
}
