// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  createDefaultSplitViewSettings,
  isValidSplitViewFormData,
  parseSplitViewConfig,
  parseSplitViewPaneSizes,
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_ENABLED,
  resolveEnabledFromPrefs,
} from "../../src/app/splitview/splitViewSettingsLogic.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function assertDeepEquals(
  actual: unknown,
  expected: unknown,
  message: string,
): void {
  const actualJson = JSON.stringify(actual);
  const expectedJson = JSON.stringify(expected);
  if (actualJson !== expectedJson) {
    throw new Error(
      `${message}: expected=${expectedJson} actual=${actualJson}`,
    );
  }
}

const tests: TestCase[] = [
  {
    name: "resolveEnabledFromPrefs requires both prefs to be true",
    fn: () => {
      assertEquals(resolveEnabledFromPrefs(true, true), true, "both true");
      assertEquals(resolveEnabledFromPrefs(false, true), false, "floorp false");
      assertEquals(
        resolveEnabledFromPrefs(true, false),
        false,
        "browser false",
      );
      assertEquals(resolveEnabledFromPrefs(null, null), false, "both null");
      assertEquals(resolveEnabledFromPrefs(true, null), false, "browser null");
    },
  },
  {
    name: "parseSplitViewConfig falls back to defaults for invalid data",
    fn: () => {
      assertDeepEquals(parseSplitViewConfig(null), {
        layout: "horizontal",
        maxPanes: 4,
      }, "null config");
      assertDeepEquals(parseSplitViewConfig("{}"), {
        layout: "horizontal",
        maxPanes: 4,
      }, "empty config");
      assertDeepEquals(
        parseSplitViewConfig('{"layout":"vertical","maxPanes":3}'),
        {
          layout: "vertical",
          maxPanes: 3,
        },
        "valid vertical config",
      );
    },
  },
  {
    name: "parseSplitViewPaneSizes falls back to defaults for invalid data",
    fn: () => {
      assertDeepEquals(parseSplitViewPaneSizes(null), {
        flexRatios: [0.5, 0.5],
        gridColRatio: 0.5,
        gridRowRatio: 0.5,
      }, "null pane sizes");
      assertDeepEquals(
        parseSplitViewPaneSizes(
          '{"flexRatios":[0.6,0.4],"gridColRatio":0.6,"gridRowRatio":0.4}',
        ),
        {
          flexRatios: [0.6, 0.4],
          gridColRatio: 0.6,
          gridRowRatio: 0.4,
        },
        "valid pane sizes",
      );
    },
  },
  {
    name: "parseSplitViewPaneSizes normalizes invalid ratio values",
    fn: () => {
      assertDeepEquals(
        parseSplitViewPaneSizes(
          '{"flexRatios":["bad",0.4],"gridColRatio":null,"gridRowRatio":1.5}',
        ),
        {
          flexRatios: [0.4],
          gridColRatio: 0.5,
          gridRowRatio: 1,
        },
        "mixed invalid ratios",
      );
      assertDeepEquals(
        parseSplitViewPaneSizes('{"flexRatios":["x","y"],"gridColRatio":-0.2}'),
        {
          flexRatios: [0.5, 0.5],
          gridColRatio: 0,
          gridRowRatio: 0.5,
        },
        "all invalid flex ratios",
      );
    },
  },
  {
    name: "isValidSplitViewFormData rejects incomplete data",
    fn: () => {
      assertEquals(isValidSplitViewFormData({}), false, "empty object");
      assertEquals(
        isValidSplitViewFormData({
          enabled: true,
          layout: "horizontal",
        }),
        false,
        "incomplete form",
      );
      assertEquals(
        isValidSplitViewFormData(createDefaultSplitViewSettings()),
        true,
        "default settings",
      );
    },
  },
  {
    name: "pref constants match chrome split-view module",
    fn: () => {
      assertEquals(
        PREF_SPLIT_VIEW_ENABLED,
        "floorp.splitView.enabled",
        "floorp pref",
      );
      assertEquals(
        PREF_BROWSER_SPLIT_VIEW_ENABLED,
        "browser.tabs.splitView.enabled",
        "browser pref",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("splitViewSettingsLogic.test.ts", tests);
}
