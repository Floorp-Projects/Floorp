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

const tests: TestCase[] = [
  {
    name: "resolveEnabledFromPrefs requires both prefs to be true",
    fn: () => {
      assertEquals(resolveEnabledFromPrefs(true, true), true);
      assertEquals(resolveEnabledFromPrefs(false, true), false);
      assertEquals(resolveEnabledFromPrefs(true, false), false);
      assertEquals(resolveEnabledFromPrefs(null, null), false);
      assertEquals(resolveEnabledFromPrefs(true, null), false);
    },
  },
  {
    name: "parseSplitViewConfig falls back to defaults for invalid data",
    fn: () => {
      assertEquals(parseSplitViewConfig(null), {
        layout: "horizontal",
        maxPanes: 4,
      });
      assertEquals(parseSplitViewConfig("{}"), {
        layout: "horizontal",
        maxPanes: 4,
      });
      assertEquals(
        parseSplitViewConfig('{"layout":"vertical","maxPanes":3}'),
        {
          layout: "vertical",
          maxPanes: 3,
        },
      );
    },
  },
  {
    name: "parseSplitViewPaneSizes falls back to defaults for invalid data",
    fn: () => {
      assertEquals(parseSplitViewPaneSizes(null), {
        flexRatios: [0.5, 0.5],
        gridColRatio: 0.5,
        gridRowRatio: 0.5,
      });
      assertEquals(
        parseSplitViewPaneSizes(
          '{"flexRatios":[0.6,0.4],"gridColRatio":0.6,"gridRowRatio":0.4}',
        ),
        {
          flexRatios: [0.6, 0.4],
          gridColRatio: 0.6,
          gridRowRatio: 0.4,
        },
      );
    },
  },
  {
    name: "parseSplitViewPaneSizes normalizes invalid ratio values",
    fn: () => {
      assertEquals(
        parseSplitViewPaneSizes(
          '{"flexRatios":["bad",0.4],"gridColRatio":null,"gridRowRatio":1.5}',
        ),
        {
          flexRatios: [0.4],
          gridColRatio: 0.5,
          gridRowRatio: 1,
        },
      );
      assertEquals(
        parseSplitViewPaneSizes('{"flexRatios":["x","y"],"gridColRatio":-0.2}'),
        {
          flexRatios: [0.5, 0.5],
          gridColRatio: 0,
          gridRowRatio: 0.5,
        },
      );
    },
  },
  {
    name: "isValidSplitViewFormData rejects incomplete data",
    fn: () => {
      assertEquals(isValidSplitViewFormData({}), false);
      assertEquals(
        isValidSplitViewFormData({
          enabled: true,
          layout: "horizontal",
        }),
        false,
      );
      assertEquals(
        isValidSplitViewFormData(createDefaultSplitViewSettings()),
        true,
      );
    },
  },
  {
    name: "pref constants match chrome split-view module",
    fn: () => {
      assertEquals(PREF_SPLIT_VIEW_ENABLED, "floorp.splitView.enabled");
      assertEquals(
        PREF_BROWSER_SPLIT_VIEW_ENABLED,
        "browser.tabs.splitView.enabled",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("splitViewSettingsLogic.test.ts", tests);
}
