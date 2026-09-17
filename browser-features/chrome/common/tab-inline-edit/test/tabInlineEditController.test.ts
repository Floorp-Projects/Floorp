// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  buildInlineTabLoadOptions,
  getSafeInlineTabPrefill,
  type InlineBrowser,
  type InlineGBrowser,
  type InlineTab,
  isDangerousInlineUrl,
  normalizeInlineTabPrefill,
  openInlineTabUrlEditorForSelectedTab,
  resolveInlineTabUrlInput,
  TAB_INLINE_EDIT_CONTEXT_ITEM_ID,
  TAB_INLINE_EDIT_INPUT_ID,
  TabInlineEditController,
  type TabInlineEditControllerDependencies,
} from "../controller.ts";
import {
  isTabInlineEditEnabled,
  TabInlineEditLifecycle,
  type TabInlineEditPrefService,
} from "../index.ts";

type RectState = {
  left: number;
  top: number;
  width: number;
  height: number;
};

type ControllerHarness = {
  controller: TabInlineEditController;
  root: HTMLElement;
  menu: HTMLElement;
  tabContainer: HTMLElement;
  selectedTab: InlineTab;
  contextTab: InlineTab;
  gBrowser: InlineGBrowser;
  selectedBrowser: InlineBrowser;
  contextBrowser: InlineBrowser;
  dependencies: Partial<TabInlineEditControllerDependencies>;
  setContextTab: (tab: InlineTab | null) => void;
  setFixup: (fixup: TabInlineEditControllerDependencies["fixup"]) => void;
  loads: Array<{
    url: string;
    where: "current";
    options: Parameters<
      TabInlineEditControllerDependencies["openTrustedLinkIn"]
    >[2];
  }>;
  cleanup: () => void;
};

function createHtmlElement<K extends keyof HTMLElementTagNameMap>(
  tagName: K,
): HTMLElementTagNameMap[K] {
  return document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    tagName,
  ) as HTMLElementTagNameMap[K];
}

function createTab(url: string, rect: RectState): InlineTab {
  const tab = createHtmlElement("div") as InlineTab;
  tab.setAttribute("data-test-url", url);
  Object.assign(tab, { linkedPanel: `panel-${Math.random()}` });
  Object.defineProperty(tab, "getBoundingClientRect", {
    configurable: true,
    value: () =>
      DOMRect.fromRect({
        x: rect.left,
        y: rect.top,
        width: rect.width,
        height: rect.height,
      }),
  });
  return tab;
}

function createHarness(): ControllerHarness {
  const root = createHtmlElement("div");
  const menu = createHtmlElement("div");
  menu.id = `test-${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`;
  const tabContainer = createHtmlElement("div");
  root.append(menu, tabContainer);
  const documentElement = document.documentElement;
  assert(documentElement, "browser test document should have a root element");
  documentElement.appendChild(root);

  const selectedRect = { left: 20, top: 30, width: 140, height: 32 };
  const contextRect = { left: 180, top: 30, width: 150, height: 32 };
  const selectedTab = createTab("https://selected.example/", selectedRect);
  const contextTab = createTab("https://context.example/", contextRect);
  tabContainer.append(selectedTab, contextTab);

  const selectedBrowser: InlineBrowser = {
    currentURI: { spec: "https://selected.example/" },
    focus: () => {},
  };
  const contextBrowser: InlineBrowser = {
    currentURI: { spec: "https://context.example/" },
    focus: () => {},
  };
  const browserByTab = new Map<InlineTab, InlineBrowser>([
    [selectedTab, selectedBrowser],
    [contextTab, contextBrowser],
  ]);
  const gBrowser: InlineGBrowser = {
    tabs: [selectedTab, contextTab],
    selectedTab,
    selectedTabs: [selectedTab],
    selectedBrowser,
    tabContainer,
    getBrowserForTab: (tab) => {
      const browser = browserByTab.get(tab);
      if (!browser) {
        throw new Error("unexpected test tab");
      }
      return browser;
    },
  };

  let currentContextTab: InlineTab | null = contextTab;
  let currentFixup: TabInlineEditControllerDependencies["fixup"] = (input) =>
    Promise.resolve({ url: input });
  const loads: ControllerHarness["loads"] = [];
  const principal = { isSystemPrincipal: true };
  const dependencies: Partial<TabInlineEditControllerDependencies> = {
    gBrowser,
    contextMenu: menu,
    overlayParent: root,
    getContextTab: () => currentContextTab,
    sessionStore: {
      getLazyTabValue: (tab) => tab.getAttribute("data-test-url") ?? undefined,
    },
    fixup: (input) => currentFixup(input),
    openTrustedLinkIn: (url, where, options) => {
      loads.push({ url, where, options });
    },
    getSystemPrincipal: () => principal,
    translate: (_key, fallback) => fallback,
  };
  const controller = new TabInlineEditController(window, dependencies);

  return {
    controller,
    root,
    menu,
    tabContainer,
    selectedTab,
    contextTab,
    gBrowser,
    selectedBrowser,
    contextBrowser,
    dependencies,
    setContextTab: (tab) => {
      currentContextTab = tab;
    },
    setFixup: (fixup) => {
      currentFixup = fixup;
    },
    loads,
    cleanup: () => {
      controller.destroy();
      root.remove();
      document.getElementById(TAB_INLINE_EDIT_INPUT_ID)?.remove();
      document.getElementById(TAB_INLINE_EDIT_CONTEXT_ITEM_ID)?.remove();
    },
  };
}

class TestPrefService implements TabInlineEditPrefService {
  private readonly observers = new Set<() => void>();

  constructor(private value: boolean) {}

  getBoolPref(_name: string, _fallback: boolean): boolean {
    return this.value;
  }

  addObserver(_name: string, observer: () => void): void {
    this.observers.add(observer);
  }

  removeObserver(_name: string, observer: () => void): void {
    this.observers.delete(observer);
  }

  set(value: boolean): void {
    this.value = value;
    for (const observer of this.observers) {
      observer();
    }
  }
}

function inputElement(): HTMLInputElement | null {
  return document.getElementById(
    TAB_INLINE_EDIT_INPUT_ID,
  ) as HTMLInputElement | null;
}

function testDangerousSchemeNormalization(): void {
  const dangerous = [
    "javascript:alert(1)",
    "  JaVaScRiPt:alert(1)",
    "java\tscript:alert(1)",
    "j a v a s c r i p t:alert(1)",
    "data:text/html,hello",
    "da\r\nta:text/html,hello",
    "\u0000d\u001fa\u007ft\u0009a:text/plain,hello",
  ];
  for (const value of dangerous) {
    assert(
      isDangerousInlineUrl(value),
      `should reject ${JSON.stringify(value)}`,
    );
  }
  assert(
    !isDangerousInlineUrl("https://example.com/javascript:data"),
    "safe URL containing scheme words should remain allowed",
  );
}

async function testRawAndPostFixupRejection(): Promise<void> {
  let fixupCalls = 0;
  const rawRejected = await resolveInlineTabUrlInput(
    " j\tavascript:alert(1) ",
    (input) => {
      fixupCalls++;
      return Promise.resolve({ url: input });
    },
  );
  assertEquals(rawRejected, null, "dangerous raw input should be rejected");
  assertEquals(fixupCalls, 0, "raw rejection should happen before fixup");

  const fixedRejected = await resolveInlineTabUrlInput(
    "safe search",
    () => Promise.resolve({ url: "d\u0000a\tt a:text/html,unsafe" }),
  );
  assertEquals(
    fixedRejected,
    null,
    "dangerous post-fixup URL should be rejected",
  );

  const blankRejected = await resolveInlineTabUrlInput(
    " \r\n ",
    () =>
      Promise.resolve({
        url: "https://should-not-run.example/",
      }),
  );
  assertEquals(blankRejected, null, "blank input should be rejected");
}

function testLazySafePrefill(): void {
  const lazyTab = createTab("https://lazy.example/", {
    left: 0,
    top: 0,
    width: 100,
    height: 30,
  });
  lazyTab.setAttribute("pending", "true");
  let browserReads = 0;
  const gBrowser: InlineGBrowser = {
    getBrowserForTab: () => {
      browserReads++;
      throw new Error("lazy browser must not be requested during prefill");
    },
  };
  const prefill = getSafeInlineTabPrefill(lazyTab, gBrowser, {
    getLazyTabValue: () => "https://lazy.example/session",
  });
  assertEquals(prefill, "https://lazy.example/session", "use lazy session URL");
  assertEquals(browserReads, 0, "lazy prefill must not wake/read the browser");

  assertEquals(
    normalizeInlineTabPrefill("about:blank"),
    "",
    "initial about:blank should be empty",
  );
  assertEquals(
    normalizeInlineTabPrefill("about:config"),
    "about:config",
    "other explicit about URLs should remain literal",
  );
}

function testExactLoadOptions(): void {
  const targetBrowser = { currentURI: { spec: "about:blank" } };
  const principal = { isSystemPrincipal: true };
  const postData = { marker: "post" };
  const options = buildInlineTabLoadOptions(
    targetBrowser,
    principal,
    postData,
  );

  assertEquals(options.targetBrowser, targetBrowser, "targetBrowser is exact");
  assertEquals(
    options.triggeringPrincipal,
    principal,
    "System Principal is explicit",
  );
  assertEquals(
    options.allowInheritPrincipal,
    false,
    "principal cannot inherit",
  );
  assertEquals(options.allowThirdPartyFixup, true, "third-party fixup is on");
  assertEquals(options.indicateErrorPageLoad, true, "error load is indicated");
  assertEquals(
    options.allowPinnedTabHostChange,
    true,
    "pinned tab host change is allowed in-place",
  );
  assertEquals(options.postData, postData, "keyword POST data is preserved");
  assert(
    !Object.hasOwn(options, "initiatedByURLBar"),
    "load must not claim URLbar initiation",
  );
  assert(!Object.hasOwn(options, "userContextId"), "container ID is untouched");
  assert(!Object.hasOwn(options, "private"), "private identity is untouched");
}

function testContextAndKeyboardTargets(): void {
  const harness = createHarness();
  try {
    assert(
      harness.controller.openForContextTab(),
      "context action should open",
    );
    assertEquals(
      inputElement()?.value,
      "https://context.example/",
      "context invocation uses exactly contextTab",
    );
    assertEquals(
      harness.gBrowser.selectedTab,
      harness.selectedTab,
      "editing inactive context tab must not select it",
    );

    inputElement()?.dispatchEvent(
      new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
    );
    assertEquals(inputElement(), null, "Escape removes the editor");

    assert(
      harness.controller.openForSelectedTab(),
      "keyboard action should open",
    );
    assertEquals(
      inputElement()?.value,
      "https://selected.example/",
      "keyboard invocation uses exactly selectedTab",
    );

    inputElement()?.dispatchEvent(new FocusEvent("blur"));
    assertEquals(
      inputElement(),
      null,
      "blur cancels without leaving an editor",
    );
    assertEquals(harness.loads.length, 0, "Escape/blur never navigate");
  } finally {
    harness.cleanup();
  }
}

function testMissingAndMultiselectNoOp(): void {
  const harness = createHarness();
  try {
    harness.setContextTab(null);
    assertEquals(
      harness.controller.openForContextTab(),
      false,
      "missing context target should no-op",
    );

    harness.setContextTab(harness.contextTab);
    harness.contextTab.setAttribute("multiselected", "true");
    assertEquals(
      harness.controller.openForContextTab(),
      false,
      "multiselected context target should no-op",
    );
    harness.contextTab.removeAttribute("multiselected");
    harness.gBrowser.selectedTabs = [harness.selectedTab, harness.contextTab];
    assertEquals(
      harness.controller.openForSelectedTab(),
      false,
      "keyboard action should no-op while tabs are multiselected",
    );
    assertEquals(inputElement(), null, "no-op paths create no overlay");
  } finally {
    harness.cleanup();
  }
}

async function testEnterFixesAndLoadsExactTarget(): Promise<void> {
  const harness = createHarness();
  try {
    const postData = { query: "encoded" };
    const fixupInputs: string[] = [];
    harness.setFixup((input) => {
      fixupInputs.push(input);
      return Promise.resolve({
        url: "https://search.example/?q=two%20words",
        postData,
      });
    });
    assert(
      harness.controller.openForContextTab(),
      "context editor should open",
    );
    const input = inputElement();
    assert(input, "editor input should exist");
    input.value = "  two words  ";
    input.dispatchEvent(
      new KeyboardEvent("keydown", { key: "Enter", bubbles: true }),
    );
    assertEquals(inputElement(), null, "Enter removes the editor immediately");

    await Promise.resolve();
    await Promise.resolve();
    assertEquals(fixupInputs[0], "two words", "input is trimmed before fixup");
    assertEquals(harness.loads.length, 1, "Enter performs one navigation");
    const load = harness.loads[0]!;
    assertEquals(load.where, "current", "load stays in the target tab");
    assertEquals(
      load.url,
      "https://search.example/?q=two%20words",
      "post-fixup URL is loaded",
    );
    assertEquals(
      load.options.targetBrowser,
      harness.contextBrowser,
      "inactive context browser is the explicit target",
    );
    assertEquals(
      load.options.postData,
      postData,
      "keyword POST data is passed",
    );
    assertEquals(
      harness.gBrowser.selectedTab,
      harness.selectedTab,
      "current-target load must not select the inactive context tab",
    );
  } finally {
    harness.cleanup();
  }
}

function testTabCloseAndDestroyCleanup(): void {
  const harness = createHarness();
  try {
    assert(
      harness.menu.querySelector(`#${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`),
      "enabled controller should create the context item",
    );
    assert(harness.controller.openForContextTab(), "editor should open");
    harness.contextTab.dispatchEvent(
      new CustomEvent("TabClose", { bubbles: true }),
    );
    assertEquals(inputElement(), null, "target TabClose removes the editor");

    assert(harness.controller.openForSelectedTab(), "editor should reopen");
    harness.controller.destroy();
    assertEquals(inputElement(), null, "destroy removes an open editor");
    assertEquals(
      harness.menu.querySelector(`#${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`),
      null,
      "destroy removes the context item",
    );
  } finally {
    harness.cleanup();
  }
}

function testPrefOffAndLiveLifecycle(): void {
  assertEquals(
    isTabInlineEditEnabled({
      getBoolPref: () => {
        throw new Error("missing or invalid pref");
      },
    }),
    false,
    "missing or invalid pref should fail closed",
  );

  const harness = createHarness();
  harness.controller.destroy();
  const prefs = new TestPrefService(false);
  const lifecycle = new TabInlineEditLifecycle(window, {
    prefs,
    createController: (win) =>
      new TabInlineEditController(win, harness.dependencies),
  });
  try {
    assertEquals(
      harness.menu.querySelector(`#${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`),
      null,
      "pref off should create no context item",
    );
    assertEquals(
      openInlineTabUrlEditorForSelectedTab(window),
      false,
      "keyboard action should fail closed while pref is off",
    );

    prefs.set(true);
    assert(
      harness.menu.querySelector(`#${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`),
      "live enable should create the controller and context item",
    );
    assertEquals(
      openInlineTabUrlEditorForSelectedTab(window),
      true,
      "live enable should make the keyboard action available",
    );
    assert(inputElement(), "enabled controller should create an editor");

    prefs.set(false);
    assertEquals(inputElement(), null, "live disable should remove the editor");
    assertEquals(
      harness.menu.querySelector(`#${TAB_INLINE_EDIT_CONTEXT_ITEM_ID}`),
      null,
      "live disable should remove the context item",
    );
    assertEquals(
      openInlineTabUrlEditorForSelectedTab(window),
      false,
      "keyboard action should fail closed again after live disable",
    );
  } finally {
    lifecycle.destroy();
    harness.cleanup();
  }
}

async function testOverlayRepositionsFromTargetRect(): Promise<void> {
  const harness = createHarness();
  try {
    let rect = { left: 100, top: 80, width: 180, height: 34 };
    Object.defineProperty(harness.contextTab, "getBoundingClientRect", {
      configurable: true,
      value: () =>
        DOMRect.fromRect({
          x: rect.left,
          y: rect.top,
          width: rect.width,
          height: rect.height,
        }),
    });
    assert(harness.controller.openForContextTab(), "editor should open");
    const input = inputElement();
    assert(input, "editor input should exist");
    assertEquals(input.style.left, "100px", "left comes from target tab rect");
    assertEquals(input.style.top, "80px", "top comes from target tab rect");

    rect = { left: 240, top: 160, width: 260, height: 40 };
    globalThis.dispatchEvent(new Event("resize"));
    await new Promise<void>((resolve) =>
      globalThis.requestAnimationFrame(() => resolve())
    );
    assertEquals(input.style.left, "240px", "resize updates horizontal anchor");
    assertEquals(input.style.top, "160px", "resize updates vertical anchor");
    assertEquals(
      input.style.width,
      "260px",
      "target width drives overlay width",
    );
  } finally {
    harness.cleanup();
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "dangerous scheme normalization",
      fn: testDangerousSchemeNormalization,
    },
    { name: "raw and post-fixup rejection", fn: testRawAndPostFixupRejection },
    { name: "lazy-safe prefill and about semantics", fn: testLazySafePrefill },
    { name: "exact current-tab load options", fn: testExactLoadOptions },
    {
      name: "context and keyboard exact targets",
      fn: testContextAndKeyboardTargets,
    },
    {
      name: "missing and multiselect no-op",
      fn: testMissingAndMultiselectNoOp,
    },
    {
      name: "Enter fixup and exact target load",
      fn: testEnterFixesAndLoadsExactTarget,
    },
    {
      name: "tab close and destroy cleanup",
      fn: testTabCloseAndDestroyCleanup,
    },
    {
      name: "pref-off and live lifecycle",
      fn: testPrefOffAndLiveLifecycle,
    },
    {
      name: "overlay repositions from target rect",
      fn: testOverlayRepositionsFromTargetRect,
    },
  ];
  await runTests("tabInlineEditController.test.ts", tests);
}
