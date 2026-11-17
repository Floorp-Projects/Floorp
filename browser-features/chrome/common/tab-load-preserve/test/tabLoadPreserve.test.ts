// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  installTabLoadPreserveController,
  TabLoadPreserveController,
  uninstallTabLoadPreserveController,
} from "../controller.ts";
import type {
  PreserveLoadBrowser,
  PreserveLoadDocument,
  PreserveLoadProgressListener,
  PreserveLoadRequest,
  PreserveLoadTab,
  PreserveLoadTabBrowser,
  PreserveLoadTabContainer,
  PreserveLoadURI,
  PreserveLoadUrlbar,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const STATE_START = 0x00000001;
const STATE_STOP = 0x00000010;
const STATE_IS_NETWORK = 0x00040000;
const LOCATION_CHANGE_SAME_DOCUMENT = 0x00000001;
const LOCATION_CHANGE_ERROR_PAGE = 0x00000004;

const NS_OK = 0;
const NS_BINDING_ABORTED = 0x804b0002;
const NS_BINDING_CANCELLED_OLD_LOAD = 0x805d0027;

class FakeBrowser implements PreserveLoadBrowser {
  private _userTypedValue: string | null = null;
  private _startedLoadSinceTyping = false;
  currentURI: PreserveLoadURI | null;
  isNavigating = false;
  readonly fixupLoads: Array<{
    uriString: string;
    loadFlags: number;
    triggeringPrincipal: unknown;
  }> = [];

  constructor(initialUri: string) {
    this.currentURI = { spec: initialUri };
  }

  get userTypedValue(): string | null {
    return this._userTypedValue;
  }

  set userTypedValue(value: string | null) {
    this._startedLoadSinceTyping = false;
    this._userTypedValue = value;
  }

  markStartedLoad(): void {
    this._startedLoadSinceTyping = true;
  }

  didStartLoadSinceLastUserTyping(): boolean {
    return this._startedLoadSinceTyping;
  }

  fixupAndLoadURIString(
    uriString: string,
    params: { loadFlags: number; triggeringPrincipal?: unknown },
  ): void {
    this.fixupLoads.push({
      uriString,
      loadFlags: params.loadFlags,
      triggeringPrincipal: params.triggeringPrincipal,
    });
  }
}

class FakeTab implements PreserveLoadTab {
  selected: boolean;
  closing = false;
  linkedBrowser: FakeBrowser;

  constructor(browser: FakeBrowser, selected = true) {
    this.linkedBrowser = browser;
    this.selected = selected;
  }
}

class FakeTabContainer implements PreserveLoadTabContainer {
  private readonly listeners = new Map<
    string,
    Set<EventListenerOrEventListenerObject>
  >();

  addEventListener(
    type: string,
    listener: EventListenerOrEventListenerObject,
  ): void {
    let set = this.listeners.get(type);
    if (!set) {
      set = new Set();
      this.listeners.set(type, set);
    }
    set.add(listener);
  }

  removeEventListener(
    type: string,
    listener: EventListenerOrEventListenerObject,
  ): void {
    this.listeners.get(type)?.delete(listener);
  }

  dispatch(type: string, target: FakeTab): void {
    const set = this.listeners.get(type);
    if (!set) return;
    const event = { target } as unknown as Event;
    for (const listener of set) {
      if (typeof listener === "function") {
        listener(event);
      } else {
        listener.handleEvent(event);
      }
    }
  }
}

class FakeReloadCommandElement {
  private readonly attrs = new Map<string, string>();

  constructor(initiallyDisabled = true) {
    if (initiallyDisabled) {
      this.attrs.set("disabled", "true");
    }
  }

  hasAttribute(name: string): boolean {
    return this.attrs.has(name);
  }

  getAttribute(name: string): string | null {
    return this.attrs.get(name) ?? null;
  }

  setAttribute(name: string, value: string): void {
    this.attrs.set(name, value);
  }

  removeAttribute(name: string): void {
    this.attrs.delete(name);
  }
}

function createHarness(initialUri = "about:home") {
  const browser = new FakeBrowser(initialUri);
  const tab = new FakeTab(browser, true);
  const tabContainer = new FakeTabContainer();
  const browserToTab = new Map<PreserveLoadBrowser, FakeTab>([[browser, tab]]);
  const listeners = new Set<PreserveLoadProgressListener>();
  const reloadCommand = new FakeReloadCommandElement(true);

  const initialTitles: Array<{
    tab: PreserveLoadTab;
    title: string;
    isURL?: boolean;
  }> = [];
  const nativeReloadWithFlagsCalls: number[] = [];
  const nativeReloadTabCalls: PreserveLoadTab[] = [];
  const nativeReloadTabsCalls: (readonly PreserveLoadTab[])[] = [];
  let nativeReloadCalls = 0;
  let urlbarSetURICalls = 0;

  const tabBrowser: PreserveLoadTabBrowser & {
    selectedTabs: FakeTab[];
  } = {
    get selectedTab() {
      return [...browserToTab.values()].find((t) => t.selected) ?? null;
    },
    get selectedTabs() {
      return [...browserToTab.values()].filter((t) => t.selected);
    },
    tabContainer,
    getTabForBrowser: (b) => browserToTab.get(b) ?? null,
    addTabsProgressListener: (listener) => {
      listeners.add(listener);
    },
    removeTabsProgressListener: (listener) => {
      listeners.delete(listener);
    },
    setInitialTabTitle: (t, title, options) => {
      initialTitles.push({ tab: t, title, isURL: options?.isURL });
    },
    reload: () => {
      nativeReloadCalls += 1;
    },
    reloadTab: (t) => {
      nativeReloadTabCalls.push(t);
    },
    reloadTabs: (tabs) => {
      nativeReloadTabsCalls.push(tabs);
    },
    reloadWithFlags: (flags) => {
      nativeReloadWithFlagsCalls.push(flags);
    },
  };

  const urlbar: PreserveLoadUrlbar = {
    setURI: () => {
      urlbarSetURICalls += 1;
    },
  };

  const doc: PreserveLoadDocument = {
    getElementById: (id) =>
      id === "Browser:Reload"
        ? (reloadCommand as unknown as Element)
        : null,
  };

  const host = {};
  const controller = installTabLoadPreserveController(
    {
      tabBrowser,
      urlbar,
      document: doc,
      isBlankPageURL: (u) =>
        u === "about:blank" || u === "about:home" || u === "about:newtab",
      isInitialPage: (u) =>
        u === "about:blank" || u === "about:home" || u === "about:newtab",
      isSuccessCode: (status) => (status >>> 0) < 0x80000000,
      getSystemPrincipal: () => "system-principal",
      cancelledOldLoadStatus: NS_BINDING_CANCELLED_OLD_LOAD,
    },
    host,
  );

  function addExtraTab(uri: string, selected = false): {
    tab: FakeTab;
    browser: FakeBrowser;
  } {
    const b = new FakeBrowser(uri);
    const t = new FakeTab(b, selected);
    browserToTab.set(b, t);
    return { tab: t, browser: b };
  }

  function emitStateChange(
    targetBrowser: FakeBrowser,
    flags: number,
    status: number,
    request: PreserveLoadRequest | null,
    isTopLevel = true,
  ): void {
    for (const listener of listeners) {
      listener.onStateChange?.(
        targetBrowser,
        { isTopLevel },
        request,
        flags,
        status,
      );
    }
  }

  function emitLocationChange(
    targetBrowser: FakeBrowser,
    spec: string,
    flags = 0,
    isTopLevel = true,
  ): void {
    if ((flags & LOCATION_CHANGE_SAME_DOCUMENT) === 0 && isTopLevel) {
      targetBrowser.currentURI = { spec };
    }
    for (const listener of listeners) {
      listener.onLocationChange?.(
        targetBrowser,
        { isTopLevel },
        null,
        { spec },
        flags,
      );
    }
  }

  function simulateFirefoxTabbrowserAbortClear(
    targetBrowser: FakeBrowser,
    tabIsEmpty: boolean,
  ): void {
    if (!tabIsEmpty) {
      targetBrowser.userTypedValue = null;
    }
  }

  return {
    browser,
    tab,
    tabBrowser,
    tabContainer,
    reloadCommand,
    controller,
    host,
    initialTitles,
    addExtraTab,
    emitStateChange,
    emitLocationChange,
    simulateFirefoxTabbrowserAbortClear,
    getUrlbarSetURICalls: () => urlbarSetURICalls,
    getNativeReloadWithFlagsCalls: () => nativeReloadWithFlagsCalls,
    getNativeReloadTabCalls: () => nativeReloadTabCalls,
    getNativeReloadTabsCalls: () => nativeReloadTabsCalls,
    getNativeReloadCalls: () => nativeReloadCalls,
    getListenerCount: () => listeners.size,
  };
}

function testRestoresPastedUrlWhenAbortedOnNonEmptyTab(): void {
  const h = createHarness("about:home");
  h.browser.userTypedValue = "https://example.com/article";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://example.com/article" },
  });

  // Simulate Firefox Tabbrowser clearing userTypedValue when !tab.isEmpty
  h.simulateFirefoxTabbrowserAbortClear(h.browser, false);
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://example.com/article" } },
  );

  assertEquals(
    h.browser.userTypedValue,
    "https://example.com/article",
    "pasted URL should be restored into browser.userTypedValue after abort",
  );
  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    "https://example.com/article",
    "controller should record preserved URL while still on about:home",
  );
  assertEquals(
    h.getUrlbarSetURICalls(),
    1,
    "urlbar.setURI should be invoked for selected tab",
  );
  assert(
    !h.reloadCommand.hasAttribute("disabled"),
    "Browser:Reload should be enabled for preserved interrupted URL",
  );
  assertEquals(
    h.initialTitles.length,
    1,
    "initial tab title should reflect the pending URL",
  );
}

function testPreservesClickedLinkFromHomeOrNewTabWhenAborted(): void {
  const h = createHarness("about:home");
  // Clicking a tile on about:home never sets userTypedValue
  assertEquals(h.browser.userTypedValue, null, "starts with null userTypedValue");

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://floorp.app/docs" },
    URI: { spec: "https://floorp.app/docs" },
  });

  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://floorp.app/docs" } },
  );

  assertEquals(
    h.browser.userTypedValue,
    "https://floorp.app/docs",
    "clicked target URL from about:home should be saved into URL bar on abort",
  );
  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    "https://floorp.app/docs",
    "controller should preserve clicked URL",
  );
  assertEquals(h.getUrlbarSetURICalls(), 1, "urlbar should refresh");
}

function testPreservesNewTabLinkWithOpenerWhenTabNotEmpty(): void {
  // Middle-clicking a link from a website creates about:blank with content opener,
  // making tab.isEmpty === false in Firefox.
  const h = createHarness("about:blank");
  h.browser.userTypedValue = "https://news.ycombinator.com/";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://news.ycombinator.com/" },
  });

  h.simulateFirefoxTabbrowserAbortClear(h.browser, false);
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://news.ycombinator.com/" } },
  );

  assertEquals(
    h.browser.userTypedValue,
    "https://news.ycombinator.com/",
    "new tab opened from webpage should keep URL after abort",
  );
  assert(
    !h.reloadCommand.hasAttribute("disabled"),
    "reload command should be enabled on about:blank with preserved URL",
  );
}

function testReloadRetriesPreservedUrlInsteadOfReloadingAboutBlank(): void {
  const h = createHarness("about:blank");
  h.browser.userTypedValue = "https://example.org/test";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://example.org/test" },
  });
  h.simulateFirefoxTabbrowserAbortClear(h.browser, false);
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://example.org/test" } },
  );

  h.tabBrowser.reloadWithFlags?.(0x0100);

  assertEquals(
    h.getNativeReloadWithFlagsCalls().length,
    0,
    "native reloadWithFlags must not reload about:blank",
  );
  assertEquals(
    h.browser.fixupLoads.length,
    1,
    "fixupAndLoadURIString should be called to retry the preserved URL",
  );
  assertEquals(
    h.browser.fixupLoads[0].uriString,
    "https://example.org/test",
    "retried URL should match preserved URL",
  );
  assertEquals(
    h.browser.fixupLoads[0].triggeringPrincipal,
    "system-principal",
    "system principal should be passed for URL bar / preserved retry",
  );
  const expectedFlags = 0x100000 | 0x200000 | 0x40000 | 0x0100;
  assertEquals(
    h.browser.fixupLoads[0].loadFlags,
    expectedFlags,
    "retry should include fixup, disallow-inherit-principal, and bypass-cache flags",
  );
}

function testReloadTabRetriesPreservedUrlAndClearsWhenClearedByUser(): void {
  const h = createHarness("about:newtab");
  h.browser.userTypedValue = "https://example.org/a";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://example.org/a" },
  });
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );

  // User clears the URL bar manually before pressing reload
  h.browser.userTypedValue = "";
  h.tabBrowser.reloadTab?.(h.tab);

  assertEquals(
    h.browser.fixupLoads.length,
    0,
    "cleared URL bar should not retry old preserved URL",
  );
  assertEquals(
    h.getNativeReloadTabCalls().length,
    1,
    "normal reloadTab should run after user cleared URL bar",
  );
}

function testPreservesTypedUrlOnExistingWebpageWithoutHijackingReload(): void {
  const h = createHarness("https://already-loaded.example/");
  h.browser.userTypedValue = "https://slow-target.example/";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://slow-target.example/" },
  });
  h.simulateFirefoxTabbrowserAbortClear(h.browser, false);
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );

  assertEquals(
    h.browser.userTypedValue,
    "https://slow-target.example/",
    "pasted URL should remain in URL bar even when previous page was loaded",
  );
  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    null,
    "reload override should not apply when a real webpage is still loaded",
  );

  h.tabBrowser.reloadWithFlags?.(0);
  assertEquals(
    h.getNativeReloadWithFlagsCalls().length,
    1,
    "reloading an existing webpage should still run native reload",
  );
}

function testDoesNotOverwriteUrlBarWhenClickingLinkOnRegularWebpage(): void {
  const h = createHarness("https://site-a.example/");
  // Clicking a link on a real webpage leaves userTypedValue === null
  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://site-b.example/" },
  });
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://site-b.example/" } },
  );

  assertEquals(
    h.browser.userTypedValue,
    null,
    "aborting a link click from a regular webpage should not inject target URL",
  );
}

function testClearsPreservedStateAfterSuccessfulLocationCommit(): void {
  const h = createHarness("about:home");
  h.browser.userTypedValue = "https://example.com/";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://example.com/" },
  });
  h.emitLocationChange(h.browser, "https://example.com/");
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );

  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    null,
    "committed navigation must not keep preservedUrl",
  );
}

function testIgnoresCancelledOldLoadAndNewUserInput(): void {
  const h = createHarness("about:home");
  h.browser.userTypedValue = "https://first.example/";
  h.browser.markStartedLoad();
  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://first.example/" },
  });

  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_CANCELLED_OLD_LOAD,
    null,
  );
  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    null,
    "cancelled-old-load must not be treated as user stop",
  );

  // User types something new before abort arrives
  h.browser.userTypedValue = "https://newly-typed.example/";
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );
  assertEquals(
    h.browser.userTypedValue,
    "https://newly-typed.example/",
    "active new user typing must not be overwritten by old stopped load",
  );
}

function testBackgroundTabPreservedAndSyncsReloadOnTabSelect(): void {
  const h = createHarness("https://active.example/");
  const bg = h.addExtraTab("about:blank", false);
  bg.browser.userTypedValue = "https://background-target.example/";
  bg.browser.markStartedLoad();

  h.emitStateChange(bg.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://background-target.example/" },
  });
  h.simulateFirefoxTabbrowserAbortClear(bg.browser, false);
  h.emitStateChange(
    bg.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "https://background-target.example/" } },
  );

  assertEquals(
    bg.browser.userTypedValue,
    "https://background-target.example/",
    "background tab should preserve its interrupted URL",
  );
  assertEquals(
    h.getUrlbarSetURICalls(),
    0,
    "active tab urlbar should not be updated for background tab abort",
  );

  h.tab.selected = false;
  bg.tab.selected = true;
  h.tabContainer.dispatch("TabSelect", bg.tab);

  assert(
    !h.reloadCommand.hasAttribute("disabled"),
    "selecting background tab with preserved URL should enable Browser:Reload",
  );
}

function testBlankErrorPageDoesNotClearPreservedUrlAndDestroyCleansUp(): void {
  const h = createHarness("about:blank");
  h.browser.userTypedValue = "ht!tp://invalid";
  h.browser.markStartedLoad();

  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, null);
  h.emitLocationChange(
    h.browser,
    "about:blank",
    LOCATION_CHANGE_ERROR_PAGE,
  );
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );

  assertEquals(
    h.controller.getPreservedUrl(h.tab),
    "ht!tp://invalid",
    "about:blank error page must not discard preserved URL",
  );

  uninstallTabLoadPreserveController(h.controller, h.host);
  assertEquals(
    h.getListenerCount(),
    0,
    "destroy should remove progress listener",
  );

  h.tabBrowser.reloadWithFlags?.(0);
  assertEquals(
    h.getNativeReloadWithFlagsCalls().length,
    1,
    "destroy should restore original reloadWithFlags",
  );
}

function testSameDocumentAndBlankInitialLoadsDoNotPollute(): void {
  const h = createHarness("about:newtab");
  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "about:newtab" },
  });
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    { originalURI: { spec: "about:newtab" } },
  );
  assertEquals(
    h.browser.userTypedValue,
    null,
    "blank/initial page internal loads must not be stored in userTypedValue",
  );

  h.browser.userTypedValue = "https://example.com/";
  h.browser.markStartedLoad();
  h.emitStateChange(h.browser, STATE_START | STATE_IS_NETWORK, NS_OK, {
    originalURI: { spec: "https://example.com/" },
  });
  h.emitLocationChange(
    h.browser,
    "about:newtab#section",
    LOCATION_CHANGE_SAME_DOCUMENT,
  );
  h.emitStateChange(
    h.browser,
    STATE_STOP | STATE_IS_NETWORK,
    NS_BINDING_ABORTED,
    null,
  );
  assertEquals(
    h.browser.userTypedValue,
    "https://example.com/",
    "same-document location change should not mark new cross-page load committed",
  );
  assert(
    h.controller instanceof TabLoadPreserveController,
    "controller instance should be returned",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "restores pasted URL when aborted on non-empty tab",
      fn: testRestoresPastedUrlWhenAbortedOnNonEmptyTab,
    },
    {
      name: "preserves clicked link from about:home when aborted",
      fn: testPreservesClickedLinkFromHomeOrNewTabWhenAborted,
    },
    {
      name: "preserves new tab URL with content opener when tab is not empty",
      fn: testPreservesNewTabLinkWithOpenerWhenTabNotEmpty,
    },
    {
      name: "reload retries preserved URL instead of reloading about:blank",
      fn: testReloadRetriesPreservedUrlInsteadOfReloadingAboutBlank,
    },
    {
      name: "reloadTab falls back when user clears URL bar",
      fn: testReloadTabRetriesPreservedUrlAndClearsWhenClearedByUser,
    },
    {
      name: "preserves pasted URL on existing webpage without hijacking reload",
      fn: testPreservesTypedUrlOnExistingWebpageWithoutHijackingReload,
    },
    {
      name: "does not overwrite URL bar when clicking link on regular webpage",
      fn: testDoesNotOverwriteUrlBarWhenClickingLinkOnRegularWebpage,
    },
    {
      name: "clears preserved state after successful location commit",
      fn: testClearsPreservedStateAfterSuccessfulLocationCommit,
    },
    {
      name: "ignores cancelled-old-load and preserves newer user typing",
      fn: testIgnoresCancelledOldLoadAndNewUserInput,
    },
    {
      name: "preserves background tab URL and syncs reload command on TabSelect",
      fn: testBackgroundTabPreservedAndSyncsReloadOnTabSelect,
    },
    {
      name: "keeps preserved URL on about:blank error page and cleans up on destroy",
      fn: testBlankErrorPageDoesNotClearPreservedUrlAndDestroyCleansUp,
    },
    {
      name: "ignores blank/initial loads and same-document events",
      fn: testSameDocumentAndBlankInitialLoadsDoNotPollute,
    },
  ];

  await runTests("tabLoadPreserve.test.ts", tests);
}
