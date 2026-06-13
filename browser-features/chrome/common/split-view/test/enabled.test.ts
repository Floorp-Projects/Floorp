// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { isSplitViewEnabled, setSplitViewEnabled } from "../data/enabled.js";
import {
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_ENABLED,
} from "../data/types.js";

const tests: TestCase[] = [
  {
    name: "isSplitViewEnabled returns true when both prefs are true",
    fn: () => {
      Services.prefs.setBoolPref(PREF_SPLIT_VIEW_ENABLED, true);
      Services.prefs.setBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED, true);
      assert(isSplitViewEnabled(), "expected enabled when both prefs are true");
    },
  },
  {
    name: "isSplitViewEnabled returns false when floorp pref is false",
    fn: () => {
      Services.prefs.setBoolPref(PREF_SPLIT_VIEW_ENABLED, false);
      Services.prefs.setBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED, true);
      assert(
        !isSplitViewEnabled(),
        "expected disabled when floorp pref is false",
      );
    },
  },
  {
    name: "isSplitViewEnabled returns false when browser pref is false",
    fn: () => {
      Services.prefs.setBoolPref(PREF_SPLIT_VIEW_ENABLED, true);
      Services.prefs.setBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED, false);
      assert(
        !isSplitViewEnabled(),
        "expected disabled when browser pref is false",
      );
    },
  },
  {
    name: "setSplitViewEnabled syncs both prefs to false",
    fn: () => {
      setSplitViewEnabled(false);
      assertEquals(
        Services.prefs.getBoolPref(PREF_SPLIT_VIEW_ENABLED),
        false,
        "floorp pref should be false",
      );
      assertEquals(
        Services.prefs.getBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED),
        false,
        "browser pref should be false",
      );
    },
  },
  {
    name: "setSplitViewEnabled syncs both prefs to true",
    fn: () => {
      setSplitViewEnabled(true);
      assertEquals(
        Services.prefs.getBoolPref(PREF_SPLIT_VIEW_ENABLED),
        true,
        "floorp pref should be true",
      );
      assertEquals(
        Services.prefs.getBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED),
        true,
        "browser pref should be true",
      );
    },
  },
];

runTests(tests);
