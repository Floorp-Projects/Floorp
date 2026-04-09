// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import TabSleepExclusion from "../index.ts";
import { DEFAULT_SETTINGS, TAB_SLEEP_EXCLUSION_PREF } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type FakeTab = {
  undiscardable?: boolean;
  linkedBrowser?: {
    currentURI?: {
      spec: string;
    };
  };
  setAttribute: (name: string, value: string) => void;
  getAttribute: (name: string) => string | null;
  removeAttribute: (name: string) => void;
};

function createSubject(): TabSleepExclusion {
  return Object.create(TabSleepExclusion.prototype) as TabSleepExclusion;
}

function createFakeTab(url: string): FakeTab {
  const attrs = new Map<string, string>();
  return {
    undiscardable: false,
    linkedBrowser: {
      currentURI: {
        spec: url,
      },
    },
    setAttribute: (name: string, value: string) => {
      attrs.set(name, value);
    },
    getAttribute: (name: string) => attrs.get(name) ?? null,
    removeAttribute: (name: string) => {
      attrs.delete(name);
    },
  };
}

function testDefaultSettingsContract(): void {
  assertEquals(
    DEFAULT_SETTINGS.enabled,
    true,
    "default settings should be enabled",
  );
  assertEquals(
    DEFAULT_SETTINGS.patterns.length,
    0,
    "default patterns should be empty",
  );
  assertEquals(
    TAB_SLEEP_EXCLUSION_PREF,
    "floorp.tabs.sleep.exclusion",
    "pref name should stay stable",
  );
}

function testMatchPatternWildcardAndCaseInsensitive(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://sub.example.com/page", "*.example.com"),
    "wildcard should match subdomain",
  );
  assert(
    subject.matchPattern("https://EXAMPLE.com/path", "example.com"),
    "matching should be case-insensitive",
  );
  assert(
    !subject.matchPattern("https://mozilla.org", "example.com"),
    "non-matching domain should return false",
  );
}

function testUpdateTabDiscardabilitySetsAndClearsMarker(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://docs.example.com/article");
  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    true,
    "matched tab should become undiscardable",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    "true",
    "matched tab should have floorp-sleep-excluded marker",
  );

  subject.settings = {
    enabled: false,
    patterns: ["*.example.com"],
  };
  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "disabled setting should clear undiscardable",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "disabled setting should clear marker",
  );
}

function testLoadSettingsFallbackOnInvalidJson(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, "{invalid-json");
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "enabled should fallback",
    );
    assertEquals(
      subject.settings.patterns.length,
      DEFAULT_SETTINGS.patterns.length,
      "patterns should fallback",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testUpdateDoesNotClearThirdPartyUndiscardableWithoutMarker(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://mozilla.org/");
  tab.undiscardable = true;
  // Intentionally do not set floorp-sleep-excluded marker.
  subject.settings = {
    enabled: false,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    true,
    "tab undiscardable set by other logic should remain unchanged without marker",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "marker should remain absent",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "default settings contract", fn: testDefaultSettingsContract },
    {
      name: "matchPattern handles wildcard and case-insensitive match",
      fn: testMatchPatternWildcardAndCaseInsensitive,
    },
    {
      name: "updateTabDiscardability sets and clears marker",
      fn: testUpdateTabDiscardabilitySetsAndClearsMarker,
    },
    {
      name: "loadSettings falls back on invalid json",
      fn: testLoadSettingsFallbackOnInvalidJson,
    },
    {
      name: "update does not clear third-party undiscardable without marker",
      fn: testUpdateDoesNotClearThirdPartyUndiscardableWithoutMarker,
    },
  ];

  await runTests("tabSleepExclusion.test.ts", tests);
}
