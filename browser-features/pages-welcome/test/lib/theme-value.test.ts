// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  themeFromPreference,
  themeToPreference,
} from "../../../../libs/ui/theme-value.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "Gecko theme override values",
    fn: () => {
      assertEquals(themeFromPreference(0), "dark", "Gecko dark value");
      assertEquals(themeFromPreference(1), "light", "Gecko light value");
      assertEquals(themeFromPreference(2), "system", "Gecko system value");
      assertEquals(themeFromPreference(null), "system", "unset follows system");
    },
  },
  {
    name: "Saving and reloading preserves every theme",
    fn: () => {
      for (const theme of ["light", "dark", "system"] as const) {
        assertEquals(
          themeFromPreference(themeToPreference(theme)),
          theme,
          `${theme} round trip`,
        );
      }
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("theme-value.test.ts", tests);
}
