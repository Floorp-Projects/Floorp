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
  const subject = Object.create(TabSleepExclusion.prototype) as TabSleepExclusion;
  // Initialize settings to match the class field initializer
  (subject as unknown as { settings: { enabled: boolean; patterns: string[] } }).settings = {
    enabled: DEFAULT_SETTINGS.enabled,
    patterns: [...DEFAULT_SETTINGS.patterns],
  };
  return subject;
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

/**
 * PATTERN MATCHING EDGE CASES
 */
function testMatchPatternWithSpecialRegexCharacters(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  // Test that special regex characters are properly escaped
  assert(
    subject.matchPattern("https://example.com/test+more", "example.com/test+more"),
    "should match URL with + character in pattern",
  );
  assert(
    subject.matchPattern("https://example.com/path?query=1", "example.com/path?"),
    "should match URL with ? character in pattern",
  );
  assert(
    subject.matchPattern("https://example.com/file.ext", ".ext"),
    "should match URL with . character in pattern",
  );
  assert(
    subject.matchPattern("https://example.com/page(1)", "page(1)"),
    "parentheses in pattern should be treated literally",
  );
}

function testMatchPatternWithEmptyPattern(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  // Empty pattern should not match anything (or match everything as empty string is in all URLs)
  // This tests the behavior with edge case
  const result = subject.matchPattern("https://example.com", "");
  // The implementation would match empty string in any URL
  assertEquals(result, true, "empty pattern matches empty string in URL");
}

function testMatchPatternWithFullPathAndQuery(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern(
      "https://example.com/path/to/page?query=value&other=123",
      "example.com/path/*",
    ),
    "should match URL with path and query parameters",
  );
  assert(
    subject.matchPattern(
      "https://example.com/api/v1/users/123",
      "example.com/api/*/users/*",
    ),
    "should match URL with multiple wildcards in path",
  );
}

function testMatchPatternWithOnlyWildcards(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com", "https://*"),
    "wildcard should match any domain after protocol",
  );
  assert(
    subject.matchPattern("https://sub.domain.example.com/page", "*.com/*"),
    "multiple wildcards should work correctly",
  );
}

/**
 * URL EXCLUSION LOGIC
 */
function testShouldExcludeUrlWithEmptyPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: [],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://example.com"),
    false,
    "should return false when patterns array is empty",
  );
}

function testShouldExcludeUrlWithNullPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] | null };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: null as unknown as string[],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://example.com"),
    false,
    "should return false when patterns is null",
  );
}

function testShouldExcludeUrlWithMultiplePatternsOneMatch(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: ["*.google.com", "*.example.com", "*.mozilla.org"],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://docs.example.com/page"),
    true,
    "should return true when one pattern matches",
  );
}

function testShouldExcludeUrlWithMultiplePatternsNoMatch(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: ["*.google.com", "*.mozilla.org"],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://example.com"),
    false,
    "should return false when no patterns match",
  );
}

/**
 * TAB URL EXTRACTION
 */
function testGetTabUrlWithNoLinkedBrowser(): void {
  const subject = createSubject() as unknown as {
    getTabUrl: (tab: Partial<FakeTab>) => string | null;
  };

  const tab: Partial<FakeTab> = {};

  assertEquals(
    subject.getTabUrl(tab),
    null,
    "should return null when tab has no linkedBrowser",
  );
}

function testGetTabUrlWithNoCurrentURI(): void {
  const subject = createSubject() as unknown as {
    getTabUrl: (tab: Partial<FakeTab>) => string | null;
  };

  const tab: Partial<FakeTab> = {
    linkedBrowser: {},
  };

  assertEquals(
    subject.getTabUrl(tab),
    null,
    "should return null when linkedBrowser has no currentURI",
  );
}

function testGetTabUrlWithNullSpec(): void {
  const subject = createSubject() as unknown as {
    getTabUrl: (tab: Partial<FakeTab>) => string | null;
  };

  const tab: Partial<FakeTab> = {
    linkedBrowser: {
      currentURI: {
        spec: null as unknown as string,
      },
    },
  };

  assertEquals(
    subject.getTabUrl(tab),
    null,
    "should return null when currentURI.spec is null",
  );
}

function testGetTabUrlWithValidSpec(): void {
  const subject = createSubject() as unknown as {
    getTabUrl: (tab: Partial<FakeTab>) => string | null;
  };

  const tab: Partial<FakeTab> = {
    linkedBrowser: {
      currentURI: {
        spec: "https://example.com",
      },
    },
  };

  assertEquals(
    subject.getTabUrl(tab),
    "https://example.com",
    "should return the spec when valid",
  );
}

function testGetTabUrlWithExceptionHandling(): void {
  const subject = createSubject() as unknown as {
    getTabUrl: (tab: Partial<FakeTab>) => string | null;
  };

  // Create a tab that throws when accessing linkedBrowser
  const tab: Partial<FakeTab> = {};
  Object.defineProperty(tab, "linkedBrowser", {
    get() {
      throw new Error("Access denied");
    },
  });

  assertEquals(
    subject.getTabUrl(tab),
    null,
    "should return null when exception is thrown",
  );
}

/**
 * SETTINGS LOADING SCENARIOS
 */
function testLoadSettingsWithEmptyPrefValue(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, "");
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "enabled should use default when pref is empty",
    );
    assert(
      Array.isArray(subject.settings.patterns) &&
        subject.settings.patterns.length === 0,
      "patterns should be empty array (default) when pref is empty",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithPartialSettingsMissingEnabled(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    const partialSettings = { patterns: ["*.example.com"] };
    Services.prefs.setStringPref(
      TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify(partialSettings),
    );
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "enabled should use default when missing",
    );
    assert(
      Array.isArray(subject.settings.patterns) &&
        subject.settings.patterns.length === 1 &&
        subject.settings.patterns[0] === "*.example.com",
      "patterns should use provided value",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithPartialSettingsMissingPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    const partialSettings = { enabled: false };
    Services.prefs.setStringPref(
      TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify(partialSettings),
    );
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      false,
      "enabled should use provided value",
    );
    assertEquals(
      subject.settings.patterns,
      DEFAULT_SETTINGS.patterns,
      "patterns should use default when missing",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithNonArrayPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    const invalidSettings = { enabled: true, patterns: "not-an-array" };
    Services.prefs.setStringPref(
      TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify(invalidSettings),
    );
    subject.loadSettings();
    assertEquals(
      subject.settings.patterns,
      DEFAULT_SETTINGS.patterns,
      "patterns should fallback to default when not an array",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithInvalidPatternsType(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    const invalidSettings = { enabled: true, patterns: 123 };
    Services.prefs.setStringPref(
      TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify(invalidSettings),
    );
    subject.loadSettings();
    assertEquals(
      subject.settings.patterns,
      DEFAULT_SETTINGS.patterns,
      "patterns should fallback to default when type is invalid",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

/**
 * TAB UPDATE SCENARIOS
 */
function testUpdateTabDiscardabilityWithNullUrl(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://example.com");
  // Simulate null URL by removing linkedBrowser
  delete (tab as Partial<FakeTab>).linkedBrowser;

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "should not set undiscardable when URL is null",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "should not set marker when URL is null",
  );
}

function testUpdateTabDiscardabilityWhenShouldExcludeIsTrue(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://sub.example.com/page");
  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    true,
    "should set undiscardable when pattern matches",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    "true",
    "should set marker when pattern matches",
  );
}

function testUpdateTabDiscardabilityWhenShouldExcludeIsFalse(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://mozilla.org/");
  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "should not set undiscardable when pattern doesn't match",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "should not set marker when pattern doesn't match",
  );
}

function testUpdateTabDiscardabilityAlreadyMarkedAndMatching(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  // Use a URL that actually matches *.example.com pattern (needs subdomain)
  const tab = createFakeTab("https://www.example.com");
  tab.setAttribute("floorp-sleep-excluded", "true");
  tab.undiscardable = true;

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    true,
    "should remain undiscardable when already marked and still matching",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    "true",
    "should keep marker when already marked and still matching",
  );
}

function testUpdateTabDiscardabilityClearsExclusionWhenNoLongerMatching(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://mozilla.org/");
  // Pre-mark as excluded
  tab.setAttribute("floorp-sleep-excluded", "true");
  tab.undiscardable = true;

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"], // Different pattern that doesn't match
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "should clear undiscardable when pattern no longer matches",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "should clear marker when pattern no longer matches",
  );
}

/**
 * APPLY TO ALL TABS SCENARIOS
 */
function testApplyToAllTabsWithEmptyTabsArray(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    applyToAllTabs: () => void;
  };

  // Mock gBrowser with empty tabs array
  (globalThis as Record<string, unknown>).gBrowser = { tabs: [] };

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  // Should not throw
  subject.applyToAllTabs();
}

function testApplyToAllTabsWithMixedUrls(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    applyToAllTabs: () => void;
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab1 = createFakeTab("https://example.com");
  const tab2 = createFakeTab("https://mozilla.org/");
  const tab3 = createFakeTab("https://docs.example.com/page");

  (globalThis as Record<string, unknown>).gBrowser = {
    tabs: [tab1, tab2, tab3],
  };

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  let callCount = 0;
  const originalUpdate = subject.updateTabDiscardability.bind(subject);
  subject.updateTabDiscardability = (tab: FakeTab) => {
    callCount++;
    originalUpdate(tab);
  };

  subject.applyToAllTabs();
  assertEquals(
    callCount,
    3,
    "should call updateTabDiscardability for all tabs",
  );
}

/**
 * CLEANUP SCENARIOS
 */
function testCleanupWithNullPrefObserver(): void {
  const subject = createSubject() as unknown as {
    prefObserver: null;
    cleanup: () => void;
  };

  subject.prefObserver = null;

  // Should not throw
  subject.cleanup();
}

function testCleanupWithValidPrefObserver(): void {
  const subject = createSubject() as unknown as {
    prefObserver: object;
    cleanup: () => void;
  };

  subject.prefObserver = {
    observe: () => {},
    QueryInterface: () => {},
  };

  subject.cleanup();
  assertEquals(
    subject.prefObserver,
    null,
    "should set prefObserver to null after cleanup",
  );
}

/**
 * PREFERENCE OBSERVER TESTS
 */
function testSetupPrefObserverCreatesObserverWithCorrectInterface(): void {
  const subject = createSubject() as unknown as {
    prefObserver: null | object;
    setupPrefObserver: () => void;
  };

  subject.prefObserver = null;
  subject.setupPrefObserver();

  assertEquals(
    subject.prefObserver !== null,
    true,
    "should create prefObserver",
  );
  assertEquals(
    typeof (subject.prefObserver as unknown as Record<string, unknown>).observe === "function",
    true,
    "observer should have observe method",
  );
  assertEquals(
    typeof (subject.prefObserver as unknown as Record<string, unknown>).QueryInterface === "function",
    true,
    "observer should have QueryInterface method",
  );
}

function testSetupPrefObserverObserverTriggersReloadOnChange(): void {
  const subject = createSubject() as unknown as {
    prefObserver: null | { observe: (_subject: unknown, topic: string, data: string) => void };
    setupPrefObserver: () => void;
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");
  let loadSettingsCalled = false;

  subject.settings = { ...DEFAULT_SETTINGS };
  subject.loadSettings = () => {
    loadSettingsCalled = true;
  };

  try {
    subject.setupPrefObserver();
    Services.prefs.setStringPref(
      TAB_SLEEP_EXCLUSION_PREF,
      JSON.stringify({ enabled: false, patterns: ["*.test.com"] }),
    );

    // Trigger observer callback manually
    if (subject.prefObserver) {
      subject.prefObserver.observe(null, "nsPref:changed", TAB_SLEEP_EXCLUSION_PREF);
    }

    assertEquals(
      loadSettingsCalled,
      true,
      "should call loadSettings when pref changes",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testSetupPrefObserverObserverIgnoresOtherTopics(): void {
  const subject = createSubject() as unknown as {
    prefObserver: null | { observe: (_subject: unknown, topic: string, data: string) => void };
    setupPrefObserver: () => void;
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  let loadSettingsCalled = false;

  subject.settings = { ...DEFAULT_SETTINGS };
  subject.loadSettings = () => {
    loadSettingsCalled = true;
  };

  subject.setupPrefObserver();

  // Trigger observer with different topic
  if (subject.prefObserver) {
    subject.prefObserver.observe(null, "other-topic", TAB_SLEEP_EXCLUSION_PREF);
  }

  assertEquals(
    loadSettingsCalled,
    false,
    "should not call loadSettings for other topics",
  );
}

function testSetupPrefObserverObserverIgnoresOtherPrefs(): void {
  const subject = createSubject() as unknown as {
    prefObserver: null | { observe: (_subject: unknown, topic: string, data: string) => void };
    setupPrefObserver: () => void;
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  let loadSettingsCalled = false;

  subject.settings = { ...DEFAULT_SETTINGS };
  subject.loadSettings = () => {
    loadSettingsCalled = true;
  };

  subject.setupPrefObserver();

  // Trigger observer with different pref
  if (subject.prefObserver) {
    subject.prefObserver.observe(null, "nsPref:changed", "other.pref.name");
  }

  assertEquals(
    loadSettingsCalled,
    false,
    "should not call loadSettings for other prefs",
  );
}

/**
 * TAB EVENT LISTENERS TESTS
 */
function testSetupTabListenersAddsTabOpenListener(): void {
  const subject = createSubject() as unknown as {
    setupTabListeners: () => void;
  };

  let tabOpenListenerAdded = false;
  const originalAddEventListener = globalThis.addEventListener;
  const originalGBrowser = (globalThis as Record<string, unknown>).gBrowser;

  // Mock gBrowser to prevent "addTabsProgressListener is not a function" error
  (globalThis as Record<string, unknown>).gBrowser = {
    addTabsProgressListener: () => {},
  };

  globalThis.addEventListener = ((event: string, _listener: EventListener) => {
    if (event === "TabOpen") {
      tabOpenListenerAdded = true;
    }
  }) as typeof globalThis.addEventListener;

  try {
    subject.setupTabListeners();
    assertEquals(
      tabOpenListenerAdded,
      true,
      "should add TabOpen event listener",
    );
  } finally {
    globalThis.addEventListener = originalAddEventListener;
    (globalThis as Record<string, unknown>).gBrowser = originalGBrowser;
  }
}

function testSetupTabListenersAddsTabsProgressListener(): void {
  const subject = createSubject() as unknown as {
    setupTabListeners: () => void;
  };

  const mockGBrowser = {
    addTabsProgressListener: (_listener: object) => {},
  };
  const originalGBrowser = globalThis.gBrowser;

  (globalThis as Record<string, unknown>).gBrowser = mockGBrowser;

  try {
    let listenerAdded = false;
    mockGBrowser.addTabsProgressListener = (_listener: object) => {
      listenerAdded = true;
    };

    subject.setupTabListeners();
    assertEquals(
      listenerAdded,
      true,
      "should add tabs progress listener",
    );
  } finally {
    globalThis.gBrowser = originalGBrowser;
  }
}

/**
 * INITIALIZATION FLOW TESTS
 */
function testInitCallsAllSetupMethods(): void {
  const subject = createSubject() as unknown as {
    loadSettings: () => void;
    setupPrefObserver: () => void;
    setupTabListeners: () => void;
    applyToAllTabs: () => void;
    init: () => void;
  };

  let loadSettingsCalled = false;
  let setupPrefObserverCalled = false;
  let setupTabListenersCalled = false;
  let applyToAllTabsCalled = false;

  subject.loadSettings = () => {
    loadSettingsCalled = true;
  };
  subject.setupPrefObserver = () => {
    setupPrefObserverCalled = true;
  };
  subject.setupTabListeners = () => {
    setupTabListenersCalled = true;
  };
  subject.applyToAllTabs = () => {
    applyToAllTabsCalled = true;
  };

  subject.init();

  assertEquals(loadSettingsCalled, true, "should call loadSettings");
  assertEquals(setupPrefObserverCalled, true, "should call setupPrefObserver");
  assertEquals(setupTabListenersCalled, true, "should call setupTabListeners");
  assertEquals(applyToAllTabsCalled, true, "should call applyToAllTabs");
}

/**
 * ADVANCED PATTERN MATCHING TESTS
 */
function testMatchPatternWithUnicodeCharacters(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com/café", "example.com/café"),
    "should match URL with unicode characters",
  );
  assert(
    subject.matchPattern("https://example.com/日本語", "example.com/日本語"),
    "should match URL with Japanese characters",
  );
  assert(
    subject.matchPattern("https://example.com/路由", "example.com/路由"),
    "should match URL with Chinese characters",
  );
}

function testMatchPatternWithMultipleConsecutiveWildcards(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com/path", "**.com"),
    "should handle multiple consecutive wildcards",
  );
  assert(
    subject.matchPattern("https://a.b.c.example.com/page", "***.example.com"),
    "should match with many consecutive wildcards",
  );
}

function testMatchPatternWithVeryLongPattern(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  const longPattern = "a".repeat(1000) + ".example.com";
  const url = `https://${longPattern}.com/path`;

  assert(
    subject.matchPattern(url, longPattern),
    "should handle very long patterns",
  );
}

function testMatchPatternWithInvalidRegexFallback(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  // Pattern that would cause invalid regex but should fallback to includes
  const url = "https://example.com/test(more";
  const pattern = "test(more";

  assert(
    subject.matchPattern(url, pattern),
    "should fallback to simple includes when regex fails",
  );
}

function testMatchPatternWithBracketsAndBraces(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com/path[1]", "example.com/path[1]"),
    "should match URL with square brackets in pattern",
  );
  assert(
    subject.matchPattern("https://example.com/path{test}", "example.com/path{test}"),
    "should match URL with curly braces in pattern",
  );
}

function testMatchPatternWithPipeAndDollar(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com/path|other", "example.com/path|other"),
    "should match URL with pipe character in pattern",
  );
  assert(
    subject.matchPattern("https://example.com/path$value", "example.com/path$value"),
    "should match URL with dollar sign in pattern",
  );
}

/**
 * SETTINGS STATE TESTS
 */
function testUpdateTabDiscardabilityWithNullSettings(): void {
  const subject = createSubject() as unknown as {
    settings: null;
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://example.com");
  (subject as unknown as { settings: { enabled: boolean; patterns: string[] } }).settings = null as unknown as { enabled: boolean; patterns: string[] };

  // Should not throw, just handle gracefully
  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "should not set undiscardable when settings is null",
  );
}

function testUpdateTabDiscardabilityWithUndefinedPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: undefined };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://example.com");
  subject.settings = {
    enabled: true,
    patterns: undefined,
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    false,
    "should not set undiscardable when patterns is undefined",
  );
}

/**
 * COMPLEX TAB STATE TRANSITIONS
 */
function testUpdateTabDiscardabilityMultipleStateChanges(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  // Use subdomain URL so it actually matches *.example.com pattern
  const tab = createFakeTab("https://www.example.com");

  // Start with matching pattern
  subject.settings = { enabled: true, patterns: ["*.example.com"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, true, "should be undiscardable");

  // Change to non-matching pattern
  subject.settings = { enabled: true, patterns: ["*.mozilla.org"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, false, "should be discardable");

  // Change back to matching pattern
  subject.settings = { enabled: true, patterns: ["*.example.com"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, true, "should be undiscardable again");
}

function testUpdateTabDiscardabilityWithEnabledToggle(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  // Use subdomain URL so it actually matches *.example.com pattern
  const tab = createFakeTab("https://www.example.com");

  // Start with enabled
  subject.settings = { enabled: true, patterns: ["*.example.com"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, true, "should be undiscardable when enabled");

  // Disable
  subject.settings = { enabled: false, patterns: ["*.example.com"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, false, "should be discardable when disabled");

  // Re-enable
  subject.settings = { enabled: true, patterns: ["*.example.com"] };
  subject.updateTabDiscardability(tab);
  assertEquals(tab.undiscardable, true, "should be undiscardable when re-enabled");
}

function testUpdateTabDiscardabilityPreservesOtherExclusions(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  const tab = createFakeTab("https://mozilla.org/");
  // Simulate tab that has undiscardable set by another feature
  tab.undiscardable = true;
  // Don't set our marker, so we shouldn't touch it

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  subject.updateTabDiscardability(tab);
  assertEquals(
    tab.undiscardable,
    true,
    "should preserve undiscardable set by other features",
  );
  assertEquals(
    tab.getAttribute("floorp-sleep-excluded"),
    null,
    "should not set our marker",
  );
}

/**
 * EDGE CASE URLS
 */
function testMatchPatternWithFragmentIdentifiers(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com/page#section", "example.com/page"),
    "should match URL with fragment",
  );
  assert(
    subject.matchPattern("https://example.com/page#section", "example.com/page#*"),
    "should match URL with wildcard fragment",
  );
}

function testMatchPatternWithPortNumbers(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://example.com:8080/path", "example.com:8080"),
    "should match URL with explicit port",
  );
  assert(
    subject.matchPattern("https://localhost:3000", "localhost:3000"),
    "should match localhost with port",
  );
}

function testMatchPatternWithAuthCredentials(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("https://user:pass@example.com/path", "example.com"),
    "should match URL with credentials",
  );
}

function testMatchPatternWithSpecialSchemes(): void {
  const subject = createSubject() as unknown as {
    matchPattern: (url: string, pattern: string) => boolean;
  };

  assert(
    subject.matchPattern("about:preferences", "about:preferences"),
    "should match about: scheme",
  );
  assert(
    subject.matchPattern("chrome://browser/content/browser.xul", "chrome://browser"),
    "should match chrome:// scheme",
  );
  assert(
    subject.matchPattern("file:///Users/test/file.html", "file:///"),
    "should match file:// scheme",
  );
}

/**
 * PATTERN ARRAY EDGE CASES
 */
function testShouldExcludeUrlWithEmptyStringInPatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: [""],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://example.com"),
    true,
    "should match empty string pattern (matches empty string in URL)",
  );
}

function testShouldExcludeUrlWithOnlyWildcardPattern(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: ["*"],
  };

  assertEquals(
    subject.shouldExcludeUrl("https://example.com"),
    true,
    "should match any URL with wildcard only pattern",
  );
}

function testShouldExcludeUrlWithDuplicatePatterns(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    shouldExcludeUrl: (url: string) => boolean;
  };

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com", "*.example.com"],
  };

  // Use subdomain URL so it actually matches *.example.com pattern
  assertEquals(
    subject.shouldExcludeUrl("https://www.example.com"),
    true,
    "should match even with duplicate patterns",
  );
}

/**
 * LOAD SETTINGS WITH VARIOUS JSON STATES
 */
function testLoadSettingsWithMalformedObject(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, "{enabled:true}");
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "should fallback to defaults for malformed object",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithNonJsonObject(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, '"just-a-string"');
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "should fallback when JSON is not an object",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

function testLoadSettingsWithNullValue(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    loadSettings: () => void;
  };

  const original = Services.prefs.getStringPref(TAB_SLEEP_EXCLUSION_PREF, "");

  try {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, "null");
    subject.loadSettings();
    assertEquals(
      subject.settings.enabled,
      DEFAULT_SETTINGS.enabled,
      "should fallback when JSON value is null",
    );
  } finally {
    Services.prefs.setStringPref(TAB_SLEEP_EXCLUSION_PREF, original);
  }
}

/**
 * APPLY TO ALL TABS EDGE CASES
 */
function testApplyToAllTabsWithSingleTab(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    applyToAllTabs: () => void;
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  // Use subdomain URL so it actually matches *.example.com pattern
  const tab = createFakeTab("https://www.example.com");
  (globalThis as Record<string, unknown>).gBrowser = { tabs: [tab] };

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  let callCount = 0;
  const originalUpdate = subject.updateTabDiscardability.bind(subject);
  subject.updateTabDiscardability = (t: FakeTab) => {
    callCount++;
    originalUpdate(t);
  };

  subject.applyToAllTabs();
  assertEquals(callCount, 1, "should process single tab");
}

function testApplyToAllTabsWithTabsThrowingErrors(): void {
  const subject = createSubject() as unknown as {
    settings: { enabled: boolean; patterns: string[] };
    applyToAllTabs: () => void;
    updateTabDiscardability: (tab: FakeTab) => void;
  };

  // Use subdomain URL so it actually matches *.example.com pattern
  const tab1 = createFakeTab("https://www.example.com");
  const tab2 = createFakeTab("https://mozilla.org/");

  (globalThis as Record<string, unknown>).gBrowser = { tabs: [tab1, tab2] };

  subject.settings = {
    enabled: true,
    patterns: ["*.example.com"],
  };

  let processedCount = 0;
  const originalUpdate = subject.updateTabDiscardability.bind(subject);
  subject.updateTabDiscardability = (t: FakeTab) => {
    processedCount++;
    if (t === tab1) {
      throw new Error("Tab processing error");
    }
    originalUpdate(t);
  };

  // The implementation does NOT catch errors — loop stops at first error
  try {
    subject.applyToAllTabs();
  } catch {
    // Expected error — loop stops at tab1
  }

  assertEquals(
    processedCount,
    1,
    "should stop processing at first error (no try/catch in impl)",
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
    // PATTERN MATCHING EDGE CASES
    {
      name: "matchPattern with special regex characters",
      fn: testMatchPatternWithSpecialRegexCharacters,
    },
    {
      name: "matchPattern with empty pattern",
      fn: testMatchPatternWithEmptyPattern,
    },
    {
      name: "matchPattern with full path and query",
      fn: testMatchPatternWithFullPathAndQuery,
    },
    {
      name: "matchPattern with only wildcards",
      fn: testMatchPatternWithOnlyWildcards,
    },
    // URL EXCLUSION LOGIC
    {
      name: "shouldExcludeUrl with empty patterns",
      fn: testShouldExcludeUrlWithEmptyPatterns,
    },
    {
      name: "shouldExcludeUrl with null patterns",
      fn: testShouldExcludeUrlWithNullPatterns,
    },
    {
      name: "shouldExcludeUrl with multiple patterns one match",
      fn: testShouldExcludeUrlWithMultiplePatternsOneMatch,
    },
    {
      name: "shouldExcludeUrl with multiple patterns no match",
      fn: testShouldExcludeUrlWithMultiplePatternsNoMatch,
    },
    // TAB URL EXTRACTION
    {
      name: "getTabUrl with no linkedBrowser",
      fn: testGetTabUrlWithNoLinkedBrowser,
    },
    {
      name: "getTabUrl with no currentURI",
      fn: testGetTabUrlWithNoCurrentURI,
    },
    {
      name: "getTabUrl with null spec",
      fn: testGetTabUrlWithNullSpec,
    },
    {
      name: "getTabUrl with valid spec",
      fn: testGetTabUrlWithValidSpec,
    },
    {
      name: "getTabUrl with exception handling",
      fn: testGetTabUrlWithExceptionHandling,
    },
    // SETTINGS LOADING SCENARIOS
    {
      name: "loadSettings with empty pref value",
      fn: testLoadSettingsWithEmptyPrefValue,
    },
    {
      name: "loadSettings with partial settings missing enabled",
      fn: testLoadSettingsWithPartialSettingsMissingEnabled,
    },
    {
      name: "loadSettings with partial settings missing patterns",
      fn: testLoadSettingsWithPartialSettingsMissingPatterns,
    },
    {
      name: "loadSettings with non-array patterns",
      fn: testLoadSettingsWithNonArrayPatterns,
    },
    {
      name: "loadSettings with invalid patterns type",
      fn: testLoadSettingsWithInvalidPatternsType,
    },
    // TAB UPDATE SCENARIOS
    {
      name: "updateTabDiscardability with null url",
      fn: testUpdateTabDiscardabilityWithNullUrl,
    },
    {
      name: "updateTabDiscardability when shouldExclude is true",
      fn: testUpdateTabDiscardabilityWhenShouldExcludeIsTrue,
    },
    {
      name: "updateTabDiscardability when shouldExclude is false",
      fn: testUpdateTabDiscardabilityWhenShouldExcludeIsFalse,
    },
    {
      name: "updateTabDiscardability already marked and matching",
      fn: testUpdateTabDiscardabilityAlreadyMarkedAndMatching,
    },
    {
      name: "updateTabDiscardability clears exclusion when no longer matching",
      fn: testUpdateTabDiscardabilityClearsExclusionWhenNoLongerMatching,
    },
    // APPLY TO ALL TABS SCENARIOS
    {
      name: "applyToAllTabs with empty tabs array",
      fn: testApplyToAllTabsWithEmptyTabsArray,
    },
    {
      name: "applyToAllTabs with mixed urls",
      fn: testApplyToAllTabsWithMixedUrls,
    },
    // CLEANUP SCENARIOS
    {
      name: "cleanup with null prefObserver",
      fn: testCleanupWithNullPrefObserver,
    },
    {
      name: "cleanup with valid prefObserver",
      fn: testCleanupWithValidPrefObserver,
    },
    // PREFERENCE OBSERVER TESTS
    {
      name: "setupPrefObserver creates observer with correct interface",
      fn: testSetupPrefObserverCreatesObserverWithCorrectInterface,
    },
    {
      name: "setupPrefObserver observer triggers reload on change",
      fn: testSetupPrefObserverObserverTriggersReloadOnChange,
    },
    {
      name: "setupPrefObserver observer ignores other topics",
      fn: testSetupPrefObserverObserverIgnoresOtherTopics,
    },
    {
      name: "setupPrefObserver observer ignores other prefs",
      fn: testSetupPrefObserverObserverIgnoresOtherPrefs,
    },
    // TAB EVENT LISTENERS TESTS
    {
      name: "setupTabListeners adds TabOpen listener",
      fn: testSetupTabListenersAddsTabOpenListener,
    },
    {
      name: "setupTabListeners adds tabs progress listener",
      fn: testSetupTabListenersAddsTabsProgressListener,
    },
    // INITIALIZATION FLOW TESTS
    {
      name: "init calls all setup methods",
      fn: testInitCallsAllSetupMethods,
    },
    // ADVANCED PATTERN MATCHING TESTS
    {
      name: "matchPattern with unicode characters",
      fn: testMatchPatternWithUnicodeCharacters,
    },
    {
      name: "matchPattern with multiple consecutive wildcards",
      fn: testMatchPatternWithMultipleConsecutiveWildcards,
    },
    {
      name: "matchPattern with very long pattern",
      fn: testMatchPatternWithVeryLongPattern,
    },
    {
      name: "matchPattern with invalid regex fallback",
      fn: testMatchPatternWithInvalidRegexFallback,
    },
    {
      name: "matchPattern with brackets and braces",
      fn: testMatchPatternWithBracketsAndBraces,
    },
    {
      name: "matchPattern with pipe and dollar",
      fn: testMatchPatternWithPipeAndDollar,
    },
    // SETTINGS STATE TESTS
    {
      name: "updateTabDiscardability with null settings",
      fn: testUpdateTabDiscardabilityWithNullSettings,
    },
    {
      name: "updateTabDiscardability with undefined patterns",
      fn: testUpdateTabDiscardabilityWithUndefinedPatterns,
    },
    // COMPLEX TAB STATE TRANSITIONS
    {
      name: "updateTabDiscardability multiple state changes",
      fn: testUpdateTabDiscardabilityMultipleStateChanges,
    },
    {
      name: "updateTabDiscardability with enabled toggle",
      fn: testUpdateTabDiscardabilityWithEnabledToggle,
    },
    {
      name: "updateTabDiscardability preserves other exclusions",
      fn: testUpdateTabDiscardabilityPreservesOtherExclusions,
    },
    // EDGE CASE URLS
    {
      name: "matchPattern with fragment identifiers",
      fn: testMatchPatternWithFragmentIdentifiers,
    },
    {
      name: "matchPattern with port numbers",
      fn: testMatchPatternWithPortNumbers,
    },
    {
      name: "matchPattern with auth credentials",
      fn: testMatchPatternWithAuthCredentials,
    },
    {
      name: "matchPattern with special schemes",
      fn: testMatchPatternWithSpecialSchemes,
    },
    // PATTERN ARRAY EDGE CASES
    {
      name: "shouldExcludeUrl with empty string in patterns",
      fn: testShouldExcludeUrlWithEmptyStringInPatterns,
    },
    {
      name: "shouldExcludeUrl with only wildcard pattern",
      fn: testShouldExcludeUrlWithOnlyWildcardPattern,
    },
    {
      name: "shouldExcludeUrl with duplicate patterns",
      fn: testShouldExcludeUrlWithDuplicatePatterns,
    },
    // LOAD SETTINGS WITH VARIOUS JSON STATES
    {
      name: "loadSettings with malformed object",
      fn: testLoadSettingsWithMalformedObject,
    },
    {
      name: "loadSettings with non json object",
      fn: testLoadSettingsWithNonJsonObject,
    },
    {
      name: "loadSettings with null value",
      fn: testLoadSettingsWithNullValue,
    },
    // APPLY TO ALL TABS EDGE CASES
    {
      name: "applyToAllTabs with single tab",
      fn: testApplyToAllTabsWithSingleTab,
    },
    {
      name: "applyToAllTabs with tabs throwing errors",
      fn: testApplyToAllTabsWithTabsThrowingErrors,
    },
  ];

  await runTests("tabSleepExclusion.test.ts", tests);
}
