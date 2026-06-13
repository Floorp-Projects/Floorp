import { assertEquals } from "@std/assert";
import {
  createDefaultSplitViewSettings,
  isValidSplitViewFormData,
  parseSplitViewConfig,
  parseSplitViewPaneSizes,
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_ENABLED,
  resolveEnabledFromPrefs,
} from "./splitViewSettingsLogic.ts";

Deno.test("resolveEnabledFromPrefs requires both prefs to be true", () => {
  assertEquals(resolveEnabledFromPrefs(true, true), true);
  assertEquals(resolveEnabledFromPrefs(false, true), false);
  assertEquals(resolveEnabledFromPrefs(true, false), false);
  assertEquals(resolveEnabledFromPrefs(null, null), false);
  assertEquals(resolveEnabledFromPrefs(true, null), false);
});

Deno.test("parseSplitViewConfig falls back to defaults for invalid data", () => {
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
});

Deno.test("parseSplitViewPaneSizes falls back to defaults for invalid data", () => {
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
});

Deno.test("isValidSplitViewFormData rejects incomplete data", () => {
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
});

Deno.test("pref constants match chrome split-view module", () => {
  assertEquals(PREF_SPLIT_VIEW_ENABLED, "floorp.splitView.enabled");
  assertEquals(
    PREF_BROWSER_SPLIT_VIEW_ENABLED,
    "browser.tabs.splitView.enabled",
  );
});
