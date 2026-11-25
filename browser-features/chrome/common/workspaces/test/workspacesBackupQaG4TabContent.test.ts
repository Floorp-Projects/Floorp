// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g4 (H-*): contents of tabs after an additive restore.
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  settle,
  waitFor,
  withWindows,
} from "./workspace-last-tab-test-utils.ts";
import { WORKSPACE_TAB_ATTRIBUTION_ID } from "../utils/workspaces-static-names.ts";
import { formatBackupFileName } from "../utils/workspaces-backup-helpers.ts";
import type { WorkspacesBackupFuncs } from "../utils/workspaces-backup-controller.ts";
import type { RestoreBackupResult } from "../utils/workspaces-backup-service.ts";
import type {
  WorkspaceBackupFile,
  WorkspaceBackupTab,
  WorkspaceBackupWindow,
} from "../utils/backup-types.ts";
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

type QaWindow = TestWindow & { workspacesBackupFuncs?: WorkspacesBackupFuncs };

interface QaPrincipal {
  isSystemPrincipal: boolean;
  isNullPrincipal: boolean;
  isContentPrincipal: boolean;
}

interface QaBrowser {
  currentURI: { spec: string };
  contentTitle: string;
  loadURI(
    uri: { spec: string },
    options: { triggeringPrincipal: QaPrincipal },
  ): void;
}

interface QaGBrowser {
  tabs: XULElement[];
  selectedTab: XULElement;
  getBrowserForTab(tab: XULElement): QaBrowser;
  getIcon(tab: XULElement): string | null;
  addTab(url: string, options: Record<string, unknown>): XULElement;
  addTabGroup?(
    tabs: XULElement[],
    options?: Record<string, unknown>,
  ): XULElement;
  addTabSplitView?(
    tabs: XULElement[],
    options?: Record<string, unknown>,
  ): XULElement;
  removeTab(tab: XULElement): void;
}

interface SessionEntry {
  url?: string;
  triggeringPrincipal_base64?: string;
  principalToInherit_base64?: string;
  resultPrincipalURI?: string;
  originalURI?: string;
}

interface SessionTabState {
  entries?: SessionEntry[];
  index?: number;
  userTypedValue?: string;
}

const { SessionStore } = ChromeUtils.importESModule(
  "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
) as { SessionStore: { getTabState(tab: XULElement): string } };
const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
) as {
  E10SUtils: {
    serializePrincipal(p: unknown): string;
    deserializePrincipal(s: string): QaPrincipal | null;
  };
};

const PREFIX = `https://example.com/floorp-qa-g4-content/${Date.now()}`;
const PNG =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

const gb = (win: QaWindow): QaGBrowser => win.gBrowser as unknown as QaGBrowser;

function backupDir(): string {
  return PathUtils.join(PathUtils.profileDir, "workspaces", "backup");
}

async function writeBackup(file: WorkspaceBackupFile): Promise<string> {
  const fileName = formatBackupFileName(
    file.capturedAt,
    crypto.randomUUID().replace(/-/g, "").slice(0, 8),
  );
  await IOUtils.makeDirectory(backupDir(), { createAncestors: true });
  await IOUtils.write(
    PathUtils.join(backupDir(), fileName),
    new TextEncoder().encode(JSON.stringify(file)),
    { compress: true },
  );
  return fileName;
}

async function readBackup(fileName: string): Promise<WorkspaceBackupFile> {
  const bytes = await IOUtils.read(PathUtils.join(backupDir(), fileName), {
    decompress: true,
  });
  return JSON.parse(new TextDecoder().decode(bytes)) as WorkspaceBackupFile;
}

function tabsFile(windows: WorkspaceBackupWindow[]): WorkspaceBackupFile {
  return {
    version: 2,
    kind: "manual",
    mode: "tabs",
    capturedAt: Date.now() - 60_000,
    windows,
  };
}

function tab(
  url: string | null,
  extra: Partial<WorkspaceBackupTab> = {},
): WorkspaceBackupTab {
  return {
    state: url ? { entries: [{ url, title: url }], index: 1 } : null,
    title: url,
    url,
    pinned: false,
    isSelected: false,
    userContextId: 0,
    lastShownWorkspaceId: null,
    ...extra,
  };
}

function tabState(t: XULElement): SessionTabState {
  return JSON.parse(SessionStore.getTabState(t)) as SessionTabState;
}

function tabUrls(win: QaWindow, t: XULElement): string[] {
  const state = tabState(t);
  const urls = (state.entries ?? []).map((e) => e.url ?? "");
  if (state.userTypedValue) urls.push(state.userTypedValue);
  urls.push(gb(win).getBrowserForTab(t).currentURI.spec);
  return urls;
}

function findTab(win: QaWindow, url: string): XULElement | undefined {
  return gb(win).tabs.find((t) => tabUrls(win, t).includes(url));
}

function newTabs(win: QaWindow, before: XULElement[]): XULElement[] {
  return gb(win).tabs.filter((t) => !before.includes(t));
}

function browserWindows(): QaWindow[] {
  const windows: QaWindow[] = [];
  const enumerator = Services.wm.getEnumerator("navigator:browser");
  while (enumerator.hasMoreElements()) {
    const win = enumerator.getNext() as QaWindow;
    if (!win.closed) windows.push(win);
  }
  return windows;
}

/**
 * Records observations of current behaviour. Set QA_G4_NOTES to a file path to
 * collect them outside the console.
 */
async function note(id: string, value: unknown): Promise<void> {
  const line = `[QA-G4] ${id} ${JSON.stringify(value)}`;
  console.log(line);
  const path = Services.env.get("QA_G4_NOTES");
  if (path) {
    await IOUtils.writeUTF8(path, `${line}\n`, { mode: "appendOrCreate" });
  }
}

async function withCleanup(
  check: (track: (fileName: string) => void) => Promise<void>,
): Promise<void> {
  const harnessWindows = browserWindows();
  const attributions = new Map<XULElement, string | null>();
  for (const win of harnessWindows) {
    for (const t of gb(win).tabs) {
      attributions.set(t, t.getAttribute(WORKSPACE_TAB_ATTRIBUTION_ID));
    }
  }
  const funcs = harnessWindows[0]?.workspacesBackupFuncs;
  assert(funcs, "harness window exposes the backup API");
  const before = new Set((await funcs.listBackups()).map((b) => b.fileName));
  const tracked = new Set<string>();
  try {
    await check((fileName) => tracked.add(fileName));
  } finally {
    for (const win of browserWindows()) {
      if (!harnessWindows.includes(win)) win.close();
    }
    for (const backup of await funcs.listBackups()) {
      if (!before.has(backup.fileName)) tracked.add(backup.fileName);
    }
    for (const fileName of tracked) await funcs.deleteBackup(fileName);
    for (const [t, value] of attributions) {
      if (!t.isConnected) continue;
      if (value === null) t.removeAttribute(WORKSPACE_TAB_ATTRIBUTION_ID);
      else t.setAttribute(WORKSPACE_TAB_ATTRIBUTION_ID, value);
    }
  }
}

/** Opens a workspaces-disabled window, restores `file` there and checks it. */
async function restoreCrafted(
  file: WorkspaceBackupFile,
  check: (
    win: QaWindow,
    result: RestoreBackupResult,
    before: XULElement[],
  ) => Promise<void> | void,
  beforeRestore?: (win: QaWindow) => void,
): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const fileName = await writeBackup(file);
      track(fileName);
      const before = [...gb(win).tabs];
      beforeRestore?.(win);
      const result = await backup.restoreBackup(fileName);
      await settle();
      await check(win, result, before);
    }, false);
  });
}

function principalOf(entry: SessionEntry | undefined): QaPrincipal | null {
  if (!entry?.triggeringPrincipal_base64) return null;
  return E10SUtils.deserializePrincipal(entry.triggeringPrincipal_base64);
}

// H-01 / H-02 / H-05
async function testHistoryPinsSelection(): Promise<void> {
  const p1 = `${PREFIX}/pin-1`;
  const p2 = `${PREFIX}/pin-2`;
  const h = [`${PREFIX}/h-a`, `${PREFIX}/h-b`, `${PREFIX}/h-c`];
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [
        tab(p1, { pinned: true }),
        tab(p2, { pinned: true }),
        tab(h[1], {
          isSelected: true,
          state: { entries: h.map((url) => ({ url, title: url })), index: 2 },
        }),
      ],
    }]),
    (win, result) => {
      assert(result.success, "restore succeeds");
      const pin1 = findTab(win, p1);
      const pin2 = findTab(win, p2);
      assert(pin1 && pin2, "pinned tabs restored");
      assert(pin1.pinned && pin2.pinned, "H-02 both pinned");
      const tabs = gb(win).tabs;
      assert(tabs.indexOf(pin1) < tabs.indexOf(pin2), "H-02 pinned order kept");
      const hist = findTab(win, h[0]);
      assert(hist, "history tab restored");
      const state = tabState(hist);
      assertEquals(
        (state.entries ?? []).map((e) => e.url).join(","),
        h.join(","),
        "H-01 entries",
      );
      assertEquals(state.index, 2, "H-01 index");
      assertEquals(gb(win).selectedTab, hist, "H-05 selected tab restored");
    },
  );
}

// H-03 / H-04
async function testContainers(): Promise<void> {
  const personal = `${PREFIX}/container-1`;
  const missing = `${PREFIX}/container-missing`;
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [
        tab(personal, { userContextId: 1 }),
        tab(missing, { userContextId: 4242 }),
      ],
    }]),
    async (win, result) => {
      assert(
        result.success,
        `H-04 restore with deleted container succeeds ${
          JSON.stringify(result)
        }`,
      );
      const a = findTab(win, personal);
      const b = findTab(win, missing);
      assert(a && b, "both tabs restored");
      assertEquals(a.getAttribute("usercontextid"), "1", "H-03 container kept");
      await note("H-04 usercontextid", {
        attribute: b.getAttribute("usercontextid"),
        identity: (b as unknown as { userContextId?: number }).userContextId,
      });
    },
  );
}

// H-06 / H-07 / H-20
async function testLazyTitlesIcons(): Promise<void> {
  const urls = [0, 1, 2, 3, 4].map((i) => `${PREFIX}/lazy-${i}`);
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: urls.map((url, i) =>
        tab(url, {
          isSelected: i === 0,
          title: `QA saved title ${i}`,
          state: {
            entries: [{ url, title: `QA saved title ${i}` }],
            index: 1,
            image: PNG,
          },
        })
      ),
    }]),
    (win, result) => {
      assert(result.success, "restore succeeds");
      for (const [i, url] of urls.entries()) {
        const t = findTab(win, url);
        assert(t, `${url} restored`);
        if (i === 0) {
          assertEquals(gb(win).selectedTab, t, "selected tab");
          continue;
        }
        assert(t.hasAttribute("pending"), `H-07 tab ${i} stays pending`);
        assertEquals(
          t.getAttribute("label"),
          `QA saved title ${i}`,
          "H-06 saved title on pending tab",
        );
        assertEquals(gb(win).getIcon(t), PNG, "H-20 data: icon restored");
      }
    },
  );
}

// H-08 / H-18 / H-19: tabs without a session state load from their URL.
async function testStatelessTabs(): Promise<void> {
  const loads: { spec: string; principal: QaPrincipal }[] = [];
  const url = `${PREFIX}/stateless`;
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [
        tab(url, { state: null }),
        tab(null, { state: null, title: "QA null url" }),
      ],
    }]),
    async (win, result, before) => {
      assert(result.success, `restore succeeds ${JSON.stringify(result)}`);
      const added = newTabs(win, before);
      assertEquals(added.length, 2, `two tabs added ${JSON.stringify(result)}`);
      const load = loads.find((l) => l.spec === url);
      assert(load, "H-18 state=null tab loads its URL");
      assert(
        !load.principal.isSystemPrincipal,
        "H-08 web URL not loaded with system principal",
      );
      await note("H-08 principal", { isNull: load.principal.isNullPrincipal });
      const blank = added.find((t) => !tabUrls(win, t).includes(url));
      assert(blank, "H-19 url=null tab exists");
      assertEquals(
        gb(win).getBrowserForTab(blank).currentURI.spec,
        "about:blank",
        "H-19 about:blank",
      );
    },
    (win) => {
      const g = gb(win);
      const original = g.getBrowserForTab;
      // Spy on loads triggered by the restore; dropped with the window.
      g.getBrowserForTab = (t: XULElement): QaBrowser => {
        const browser = original.call(g, t);
        const load = browser.loadURI;
        browser.loadURI = (uri, options) => {
          loads.push({
            spec: uri.spec,
            principal: options.triggeringPrincipal,
          });
          browser.loadURI = load;
          return load.call(browser, uri, options);
        };
        return browser;
      };
    },
  );
}

// A-13: reader view tabs restore the article page instead of an empty tab.
async function testReaderTabs(): Promise<void> {
  const article = `${PREFIX}/reader-article`;
  const reader = `about:reader?url=${encodeURIComponent(article)}`;
  const unsafe = `about:reader?url=${encodeURIComponent("file:///etc/passwd")}`;
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [tab(reader), tab(unsafe, { state: null })],
    }]),
    (win, result, before) => {
      assert(result.success, `restore succeeds ${JSON.stringify(result)}`);
      const added = newTabs(win, before);
      assertEquals(
        added.length + (result.skippedCount ?? 0),
        2,
        `each reader tab is restored or skipped ${JSON.stringify(result)}`,
      );
      assert(
        added.some((t) => tabUrls(win, t).includes(article)),
        "A-13 reader tab restores the article URL",
      );
      assert(
        added.every((t) =>
          tabUrls(win, t).every((url) => !url.startsWith("file:"))
        ),
        "A-13 a reader wrapping an unsafe URL is not unwrapped",
      );
    },
  );
}

// H-09 / H-15: internal page and tampered principals.
async function testPrincipals(): Promise<void> {
  const tampered = `${PREFIX}/tampered`;
  const system = E10SUtils.serializePrincipal(
    Services.scriptSecurityManager.getSystemPrincipal(),
  );
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [
        tab("about:newtab"),
        tab(tampered, {
          state: {
            entries: [{
              url: tampered,
              title: "t",
              triggeringPrincipal_base64: system,
              principalToInherit_base64: system,
              resultPrincipalURI: "about:config",
              originalURI: "file:///etc/passwd",
            }],
            index: 1,
          },
        }),
      ],
    }]),
    (win, result, before) => {
      assert(result.success, "restore succeeds");
      const added = newTabs(win, before);
      const newtab = added.find((t) =>
        tabUrls(win, t).includes("about:newtab")
      );
      assert(newtab, "H-09 about:newtab restored");
      assert(
        principalOf(tabState(newtab).entries?.[0])?.isSystemPrincipal,
        "H-09 internal page uses system principal",
      );
      const t = findTab(win, tampered);
      assert(t, "tampered tab restored");
      const entry = tabState(t).entries?.[0];
      const principal = principalOf(entry);
      assert(
        !principal?.isSystemPrincipal,
        "H-15 file-provided system principal ignored",
      );
      assert(
        entry?.resultPrincipalURI !== "about:config",
        "H-15 resultPrincipalURI dropped",
      );
      assert(
        entry?.originalURI !== "file:///etc/passwd",
        "H-15 originalURI dropped",
      );
      assert(
        !entry?.principalToInherit_base64 ||
          !E10SUtils.deserializePrincipal(entry.principalToInherit_base64)
            ?.isSystemPrincipal,
        "H-15 principalToInherit dropped",
      );
    },
  );
}

// H-12 / H-13 / H-14 / H-17 (counts only)
async function testUnsafeUrls(): Promise<void> {
  const ok = `${PREFIX}/safe`;
  const hist = [`${PREFIX}/hist-a`, "file:///etc/passwd", `${PREFIX}/hist-b`];
  const unsafe = [
    "javascript:alert(1)",
    "data:text/html,<b>x</b>",
    "file:///etc/passwd",
    "about:config",
  ];
  await restoreCrafted(
    tabsFile([{
      isFocused: true,
      tabs: [
        ...unsafe.map((u) => tab(u)),
        tab(ok),
        tab(hist[2], {
          state: {
            entries: hist.map((url) => ({ url, title: url })),
            index: 3,
          },
        }),
      ],
    }]),
    (win, result, before) => {
      assert(result.success, `restore succeeds ${JSON.stringify(result)}`);
      assertEquals(
        result.success && result.skippedCount,
        4,
        "H-12/H-13 four tabs skipped",
      );
      const added = newTabs(win, before);
      assertEquals(added.length, 2, "only safe tabs added");
      for (const t of added) {
        for (const u of tabUrls(win, t)) {
          assert(!unsafe.includes(u), `H-12/H-13 ${u} not opened`);
        }
      }
      const h = findTab(win, hist[0]);
      assert(h, "history tab restored");
      const state = tabState(h);
      assertEquals(
        (state.entries ?? []).map((e) => e.url).join(","),
        `${hist[0]},${hist[2]}`,
        "H-14 file: entry dropped",
      );
      assertEquals(state.index, 2, "H-14 index remapped");
    },
  );
}

// H-10 / H-11
async function testExtensionPages(): Promise<void> {
  const { AddonManager } = ChromeUtils.importESModule(
    "resource://gre/modules/AddonManager.sys.mjs",
  ) as {
    AddonManager: {
      installTemporaryAddon(
        f: nsIFile,
      ): Promise<{ uninstall(): Promise<void> }>;
    };
  };
  const directory = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "floorp-qa-g4-ext-",
  );
  const id = `floorp-qa-g4-${crypto.randomUUID()}@tests.invalid`;
  let addon: { uninstall(): Promise<void> } | undefined;
  try {
    await IOUtils.writeUTF8(
      PathUtils.join(directory, "manifest.json"),
      JSON.stringify({
        manifest_version: 2,
        name: "QA g4",
        version: "1.0",
        browser_specific_settings: { gecko: { id } },
      }),
    );
    await IOUtils.writeUTF8(
      PathUtils.join(directory, "page.html"),
      "<!doctype html><meta charset=utf-8><title>QA g4 extension page</title>ok",
    );
    const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(directory);
    addon = await AddonManager.installTemporaryAddon(file);
    const policy = (globalThis as unknown as {
      WebExtensionPolicy: {
        getByID(id: string): { mozExtensionHostname: string } | null;
      };
    }).WebExtensionPolicy.getByID(id);
    assert(policy, "extension policy");
    const extUrl = `moz-extension://${policy.mozExtensionHostname}/page.html`;
    const goneUrl = `moz-extension://${crypto.randomUUID()}/page.html`;
    await restoreCrafted(
      tabsFile([{
        isFocused: true,
        tabs: [tab(extUrl, { isSelected: true }), tab(goneUrl)],
      }]),
      async (win, result) => {
        assert(
          result.success,
          `H-11 restore with missing extension succeeds ${
            JSON.stringify(result)
          }`,
        );
        const t = findTab(win, extUrl);
        assert(t, "H-10 extension tab restored");
        assert(findTab(win, goneUrl), "H-11 missing-extension tab created");
        assertEquals(gb(win).selectedTab, t, "extension tab selected");
        const browser = gb(win).getBrowserForTab(t);
        await waitFor(
          () => browser.contentTitle === "QA g4 extension page",
          "H-10 extension page loads",
        );
        const p = principalOf(tabState(t).entries?.[0]);
        await note("H-10 principal", {
          system: p?.isSystemPrincipal,
          content: p?.isContentPrincipal,
        });
      },
    );
  } finally {
    await addon?.uninstall();
    await IOUtils.remove(directory, { recursive: true });
  }
}

// H-16: workspaces enabled, every tab of a workspaces file rejected.
async function testAllUnsafeWorkspacesFile(): Promise<void> {
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const summary = await backup.backupNow();
      track(summary.fileName);
      const file = await readBackup(summary.fileName);
      assertEquals(file.mode, "workspaces", "captured in workspaces mode");
      for (const w of file.windows) {
        for (const t of w.tabs) {
          t.url = "file:///etc/passwd";
          t.state = { entries: [{ url: "about:config" }], index: 1 };
        }
      }
      file.capturedAt = Date.now() - 30_000;
      const crafted = await writeBackup(file);
      track(crafted);
      const allTabs = browserWindows().flatMap((w) => gb(w).tabs);
      const dataBefore = Services.prefs.getStringPref(
        "floorp.workspaces.v4.store",
        "",
      );
      const result = await backup.restoreBackup(crafted);
      await settle();
      assert(!result.success, "H-16 restore refused");
      assertEquals(result.error, "empty-backup", "H-16 empty-backup");
      for (const t of allTabs) assert(t.isConnected, "H-16 nothing closed");
      assertEquals(
        Services.prefs.getStringPref("floorp.workspaces.v4.store", ""),
        dataBefore,
        "H-16 workspace store unchanged",
      );
    }, true);
  });
}

// H-21: an SSB (PWA) window is a navigator:browser window too.
async function testSsbWindow(): Promise<void> {
  const url = `${PREFIX}/ssb`;
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const { BrowserWindowTracker } = ChromeUtils.importESModule(
        "resource:///modules/BrowserWindowTracker.sys.mjs",
      ) as {
        BrowserWindowTracker: {
          promiseOpenWindow(o: Record<string, unknown>): Promise<QaWindow>;
        };
      };
      const extra = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag2,
      );
      extra.setPropertyAsAString("ssbid", "qa-g4-ssb");
      extra.setPropertyAsAString("taskbartab", "qa-g4-ssb");
      const arg = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString,
      );
      arg.data = url;
      const args = Cc["@mozilla.org/array;1"].createInstance(
        Ci.nsIMutableArray,
      );
      args.appendElement(arg);
      args.appendElement(extra);
      const ssb = await BrowserWindowTracker.promiseOpenWindow({
        args,
        all: false,
      });
      try {
        await waitFor(
          () => !!findTab(ssb, url),
          "SSB window shows its start URL",
        );
        win.focus();
        const summary = await backup.backupNow();
        track(summary.fileName);
        const file = await readBackup(summary.fileName);
        const captured = file.windows.some((w) =>
          w.tabs.some((t) => t.url === url)
        );
        await note("H-21 ssb captured", {
          captured,
          ssbAttr: ssb.document.documentElement.getAttribute("taskbartab"),
        });
        const before = [...gb(win).tabs];
        const result = await backup.restoreBackup(summary.fileName);
        await settle();
        assert(
          result.success,
          `H-21 restore succeeds ${JSON.stringify(result)}`,
        );
        await note("H-21 restored", {
          intoCurrentWindow: newTabs(win, before).some((t) =>
            tabUrls(win, t).includes(url)
          ),
          windows: browserWindows().length,
        });
      } finally {
        if (!ssb.closed) ssb.close();
      }
    }, false);
  });
}

// H-22 / H-23: split view and tab groups survive a round trip.
async function testSplitViewAndGroups(): Promise<void> {
  const urls = [0, 1, 2, 3].map((i) => `${PREFIX}/grp-${i}`);
  await withCleanup(async (track) => {
    await withWindows(false, false, async (open) => {
      const win = (await open()) as QaWindow;
      const backup = win.workspacesBackupFuncs;
      assert(backup, "backup API");
      const g = gb(win);
      const created = urls.map((u) =>
        g.addTab(u, {
          triggeringPrincipal: Services.scriptSecurityManager
            .getSystemPrincipal(),
          inBackground: true,
        })
      );
      await settle();
      assert(typeof g.addTabGroup === "function", "tab groups supported");
      g.addTabGroup(created.slice(0, 2), { label: "QA g4" });
      let split = false;
      if (typeof g.addTabSplitView === "function") {
        g.addTabSplitView(created.slice(2, 4));
        split = true;
      }
      await settle();
      const sourceInSplit = created.filter((t) =>
        !!(t as unknown as { splitview?: unknown }).splitview
      ).length;
      const summary = await backup.backupNow();
      track(summary.fileName);
      const before = [...g.tabs];
      const result = await backup.restoreBackup(summary.fileName);
      await settle();
      assert(
        result.success,
        `H-22/H-23 restore succeeds ${JSON.stringify(result)}`,
      );
      const added = newTabs(win, before);
      for (const u of urls) {
        assert(
          added.some((t) =>
            tabUrls(win, t).includes(u)
          ),
          `${u} restored`,
        );
      }
      await note("H-22/H-23", {
        splitApi: split,
        sourceInSplit,
        restoredInGroup: added.filter((t) =>
          !!(t as unknown as { group?: unknown }).group
        ).length,
        restoredInSplit: added.filter((t) =>
          !!(t as unknown as { splitview?: unknown }).splitview
        ).length,
      });
    }, false);
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG4TabContent.test.ts", [
    {
      name: "H-01/02/05 history, pinned order and selection",
      fn: testHistoryPinsSelection,
    },
    { name: "H-03/04 containers including a deleted one", fn: testContainers },
    {
      name: "H-06/07/20 pending tabs keep title and data: icon",
      fn: testLazyTitlesIcons,
    },
    { name: "H-08/18/19 tabs without state", fn: testStatelessTabs },
    { name: "A-13 reader view tabs", fn: testReaderTabs },
    {
      name: "H-09/15 internal page and tampered principals",
      fn: testPrincipals,
    },
    { name: "H-12/13/14 unsafe URLs skipped", fn: testUnsafeUrls },
    { name: "H-10/11 extension pages", fn: testExtensionPages },
    {
      name: "H-16 all-unsafe workspaces file refused",
      fn: testAllUnsafeWorkspacesFile,
    },
    { name: "H-21 SSB window round trip", fn: testSsbWindow },
    {
      name: "H-22/23 split view and tab groups round trip",
      fn: testSplitViewAndGroups,
    },
  ]);
}
