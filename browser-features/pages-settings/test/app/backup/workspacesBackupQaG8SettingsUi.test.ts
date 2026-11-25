// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
// QA group g8 (PR #3159): Hub backup settings page UI, i18n and a11y.
import { act, createElement as h } from "react";
import { createRoot } from "react-dom/client";
import { createInstance, type i18n as I18nInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import { MemoryRouter } from "react-router-dom";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import { FloorpUIProvider } from "../../../../../libs/ui/provider.tsx";
import { rpc } from "../../../src/lib/rpc/rpc.ts";
import BackupPage from "../../../src/app/backup/page.tsx";
import type { WorkspaceBackupSummary } from "../../../src/app/backup/dataManager.ts";
import {
  buildSearchDocuments,
  normalizeSearchText,
} from "../../../src/lib/search/index.tsx";

const LOCALES = import.meta.glob("../../../src/lib/i18n/locales/*.json", {
  eager: true,
  import: "default",
}) as Record<string, object>;

function localeResources(): Record<string, { translations: object }> {
  const resources: Record<string, { translations: object }> = {};
  for (const [path, content] of Object.entries(LOCALES)) {
    const lng = path.match(/locales\/(.*)\.json$/)?.[1];
    if (lng) resources[lng] = { translations: content };
  }
  return resources;
}

async function createI18n(lng: string): Promise<I18nInstance> {
  const instance = createInstance();
  await instance.init({
    lng,
    fallbackLng: "en-US",
    initImmediate: false,
    resources: localeResources(),
    defaultNS: "translations",
    ns: ["translations"],
    interpolation: {
      escapeValue: false,
      defaultVariables: { productName: "Floorp" },
    },
    react: { useSuspense: false },
  });
  return instance;
}

interface BridgeResponse {
  requestId: string;
  success: boolean;
  error?: string;
  data?: unknown;
}

type BridgeFn = (...args: unknown[]) => Promise<BridgeResponse>;

const BRIDGE_NAMES = [
  "NRWorkspacesBackupNow",
  "NRWorkspacesListBackups",
  "NRWorkspacesRestoreBackup",
  "NRWorkspacesDeleteBackup",
  "NRWorkspacesDeleteAllBackups",
] as const;
type BridgeName = typeof BRIDGE_NAMES[number];

const ok = (data: unknown): Promise<BridgeResponse> =>
  Promise.resolve({ requestId: "qa", success: true, data });
const fail = (error: string): Promise<BridgeResponse> =>
  Promise.resolve({ requestId: "qa", success: false, error });

interface Mocks {
  config: string | null;
  workspacesEnabled: boolean;
  configReadError: boolean;
  bridges: Partial<Record<BridgeName, BridgeFn>>;
  calls: { name: BridgeName; args: unknown[] }[];
  prefWrites: { name: string; value: string }[];
}

function installMocks(mocks: Mocks): () => void {
  const target = globalThis as unknown as Record<string, unknown>;
  const saved = new Map<string, unknown>();
  for (const name of BRIDGE_NAMES) {
    saved.set(name, target[name]);
    target[name] = (...args: unknown[]) => {
      mocks.calls.push({ name, args });
      const fn = mocks.bridges[name];
      return fn ? fn(...args) : fail("not-mocked");
    };
  }
  const originalGetString = rpc.getStringPref;
  const originalSetString = rpc.setStringPref;
  const originalGetBool = rpc.getBoolPref;
  rpc.getStringPref = (name: string) => {
    assertEquals(name, "floorp.workspaces.backup.config", "only config read");
    return mocks.configReadError
      ? Promise.reject(new Error("injected read failure"))
      : Promise.resolve(mocks.config);
  };
  rpc.setStringPref = (name: string, value: string) => {
    mocks.prefWrites.push({ name, value });
    mocks.config = value;
    return Promise.resolve();
  };
  rpc.getBoolPref = (name: string) =>
    Promise.resolve(
      name === "floorp.workspaces.enabled" ? mocks.workspacesEnabled : null,
    );
  return () => {
    for (const name of BRIDGE_NAMES) {
      const value = saved.get(name);
      if (value === undefined) delete target[name];
      else target[name] = value;
    }
    rpc.getStringPref = originalGetString;
    rpc.setStringPref = originalSetString;
    rpc.getBoolPref = originalGetBool;
  };
}

function defaultMocks(backups: WorkspaceBackupSummary[] = []): Mocks {
  return {
    config: JSON.stringify({
      enabled: true,
      intervalMinutes: 15,
      retentionDays: 7,
      maxPreRestoreBackups: 10,
    }),
    workspacesEnabled: true,
    configReadError: false,
    bridges: { NRWorkspacesListBackups: () => ok(backups) },
    calls: [],
    prefWrites: [],
  };
}

const CAPTURED_AT = Date.UTC(2026, 10, 17, 8, 30, 15);

function summary(
  overrides: Partial<WorkspaceBackupSummary> = {},
): WorkspaceBackupSummary {
  return {
    id: "abcdef01",
    kind: "manual",
    mode: "workspaces",
    capturedAt: CAPTURED_AT,
    windowCount: 1,
    workspaceCount: 2,
    tabCount: 3,
    fileName: "backup-20261117-083015-abcdef01.jsonlz4",
    fileSize: 2048,
    ...overrides,
  };
}

async function flush(rounds = 6): Promise<void> {
  for (let i = 0; i < rounds; i++) {
    await act(async () => {
      await new Promise((resolve) => setTimeout(resolve, 0));
    });
  }
}

async function until(
  predicate: () => boolean,
  message: string,
  timeoutMs = 5000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!predicate() && Date.now() < deadline) await flush(1);
  assert(predicate(), message);
}

interface View {
  host: HTMLElement;
  i18n: I18nInstance;
  text(): string;
  buttons(): HTMLButtonElement[];
  button(label: string): HTMLButtonElement;
  click(element: HTMLElement): Promise<void>;
  cleanup(): void;
}

async function renderPage(mocks: Mocks, lng = "en-US"): Promise<{
  view: View;
  restore: () => void;
}> {
  const restore = installMocks(mocks);
  const i18n = await createI18n(lng);
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  await act(async () => {
    root.render(
      h(
        FloorpUIProvider,
        null,
        h(
          I18nextProvider,
          { i18n },
          h(
            MemoryRouter,
            { initialEntries: ["/features/backup"] },
            h(BackupPage),
          ),
        ),
      ),
    );
    await Promise.resolve();
  });
  await flush();
  // Dialogs render into a portal outside the host.
  const all = () => [...document.querySelectorAll<HTMLButtonElement>("button")];
  const view: View = {
    host,
    i18n,
    text: () => host.textContent ?? "",
    buttons: () => [...host.querySelectorAll<HTMLButtonElement>("button")],
    button: (label: string) => {
      const found = all().find((b) =>
        b.textContent?.trim() === label ||
        b.getAttribute("aria-label") === label
      );
      assert(found, `button "${label}" exists`);
      return found!;
    },
    click: async (element: HTMLElement) => {
      await act(async () => {
        element.click();
        await Promise.resolve();
      });
      await flush();
    },
    cleanup: () => {
      act(() => root.unmount());
      host.remove();
    },
  };
  return { view, restore };
}

async function withPage(
  mocks: Mocks,
  check: (view: View) => Promise<void>,
  lng = "en-US",
): Promise<void> {
  const { view, restore } = await renderPage(mocks, lng);
  try {
    await check(view);
  } finally {
    view.cleanup();
    restore();
  }
}

function dateText(lng: string, at = CAPTURED_AT): string {
  return new Intl.DateTimeFormat(lng, {
    dateStyle: "medium",
    timeStyle: "short",
  })
    .format(new Date(at));
}

const tests: TestCase[] = [
  {
    name:
      "P-04 workspaces disabled switches title, description and restore label",
    fn: async () => {
      const mocks = defaultMocks([summary({ mode: "tabs" })]);
      mocks.workspacesEnabled = false;
      await withPage(mocks, async (view) => {
        await until(() => view.text().includes("Tab Backup"), "tabs title");
        assert(
          view.text().includes(
            "Back up open tabs automatically, and reopen them when needed.",
          ),
          "tabs description",
        );
        assert(!view.text().includes("Workspace Backup"), "no workspace title");
        assert(
          view.buttons().some((b) => b.textContent?.trim() === "Reopen tabs"),
          "restore button reads Reopen tabs",
        );
      });
      const enabled = defaultMocks([summary()]);
      await withPage(enabled, async (view) => {
        await until(() => view.text().includes("Workspace Backup"), "ws title");
        assert(
          view.buttons().some((b) => b.textContent?.trim() === "Restore"),
          "restore button reads Restore",
        );
      });
    },
  },
  {
    name: "P-05 last backup shows none or the newest capture time",
    fn: async () => {
      await withPage(defaultMocks([]), async (view) => {
        await until(() => view.text().includes("No backups yet"), "never");
        assert(view.text().includes("No backups yet."), "empty list text");
      });
      const newest = CAPTURED_AT + 3_600_000;
      await withPage(
        defaultMocks([
          summary({ capturedAt: newest, id: "bbbbbbbb", fileName: "b" }),
          summary(),
        ]),
        async (view) => {
          await until(
            () =>
              view.text().includes(`Last backup: ${dateText("en-US", newest)}`),
            "last backup uses the first (newest) entry",
          );
        },
      );
    },
  },
  {
    name:
      "P-06/V-05 list failure shows a loading state, an alert and a working retry",
    fn: async () => {
      const mocks = defaultMocks();
      const pending = Promise.withResolvers<BridgeResponse>();
      mocks.bridges.NRWorkspacesListBackups = () => pending.promise;
      await withPage(mocks, async (view) => {
        await until(
          () =>
            [...view.host.querySelectorAll("[role=status]")].some((n) =>
              n.textContent === "Loading backups…"
            ),
          "loading status",
        );
        pending.resolve({ requestId: "qa", success: false, error: "io" });
        await until(
          () =>
            view.host.querySelector("[role=alert]")?.textContent ===
              "Failed to load backups.",
          "list error is announced with role=alert",
        );
        mocks.bridges.NRWorkspacesListBackups = () => ok([summary()]);
        await view.click(view.button("Retry"));
        await until(
          () =>
            !view.host.querySelector("[role=alert]") &&
            view.text().includes("3 tabs"),
          "retry reloads the list",
        );
      });
    },
  },
  {
    name: "V-05 page config load failure and action errors use role=alert",
    fn: async () => {
      const mocks = defaultMocks([summary()]);
      mocks.configReadError = true;
      mocks.bridges.NRWorkspacesBackupNow = () => fail("nothing-to-back-up");
      await withPage(mocks, async (view) => {
        await until(
          () =>
            [...view.host.querySelectorAll("[role=alert]")].some((node) =>
              node.textContent ===
                "Could not load settings. Please reload this page."
            ),
          "config load error alert",
        );
        mocks.configReadError = false;
        await view.click(view.button("Retry"));
        await until(
          () => !view.text().includes("Could not load settings"),
          "config retry clears the alert",
        );
        await view.click(view.button("Back up now"));
        await until(
          () =>
            [...view.host.querySelectorAll("[role=alert]")].some((node) =>
              node.textContent === "There are no tabs to back up right now."
            ),
          "action error is a mapped alert",
        );
      });
    },
  },
  {
    name: "P-07 every action button is disabled while an operation runs",
    fn: async () => {
      const mocks = defaultMocks([summary()]);
      const pending = Promise.withResolvers<BridgeResponse>();
      mocks.bridges.NRWorkspacesBackupNow = () => pending.promise;
      await withPage(mocks, async (view) => {
        await until(() => view.text().includes("3 tabs"), "list rendered");
        await view.click(view.button("Back up now"));
        const actions = [
          "Back up now",
          "Delete all backups",
          "Restore",
          "Delete",
        ];
        for (const label of actions) {
          const found = view.buttons().find((b) =>
            b.textContent?.trim() === label
          );
          assert(found?.disabled, `${label} is disabled while busy`);
        }
        pending.resolve({ requestId: "qa", success: true, data: summary() });
        await until(
          () => !view.button("Back up now").disabled,
          "buttons are released after the operation",
        );
        for (const label of actions) {
          const found = view.buttons().find((b) =>
            b.textContent?.trim() === label
          );
          assert(found && !found.disabled, `${label} is enabled again`);
        }
      });
    },
  },
  {
    name: "P-08 dates follow the UI locale (ja-JP / en-US)",
    fn: async () => {
      for (const lng of ["en-US", "ja-JP"]) {
        await withPage(defaultMocks([summary()]), async (view) => {
          await until(
            () => view.text().includes(dateText(lng)),
            `${lng} list date uses Intl format ${dateText(lng)}`,
          );
        }, lng);
      }
      assert(dateText("ja-JP") !== dateText("en-US"), "formats differ");
    },
  },
  {
    name: "P-09 file sizes are shown in B / KB / MB",
    fn: async () => {
      const cases: [number, string][] = [
        [0, "0 B"],
        [1023, "1023 B"],
        [1024, "1.0 KB"],
        [1536, "1.5 KB"],
        [3 * 1024 * 1024, "3.0 MB"],
      ];
      const backups = cases.map(([size], index) =>
        summary({
          id: `0000000${index}`,
          fileName: `f${index}`,
          fileSize: size,
          tabCount: 10 + index,
        })
      );
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("10 tabs"), "list rendered");
        for (const [index, [, expected]] of cases.entries()) {
          const desc = view.host.querySelector(`#backup-desc-0000000${index}`);
          assert(
            desc?.textContent?.endsWith(`· ${expected}`),
            `size ${expected}: got "${desc?.textContent}"`,
          );
        }
      });
    },
  },
  {
    name: "P-09 a size just below 1 MiB is not shown as 1024.0 KB",
    fn: async () => {
      const backups = [summary({ fileSize: 1024 * 1024 - 1 })];
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("3 tabs"), "list rendered");
        const desc = view.host.querySelector("#backup-desc-abcdef01");
        assert(
          !desc?.textContent?.includes("1024.0 KB"),
          `boundary size is rounded into the next unit: "${desc?.textContent}"`,
        );
      });
    },
  },
  {
    name: "P-10 restore confirms, calls the bridge and refreshes the list",
    fn: async () => {
      const target = summary();
      const preRestore = summary({
        id: "cccccccc",
        kind: "pre-restore",
        capturedAt: CAPTURED_AT + 60_000,
        fileName: "backup-pre",
      });
      const mocks = defaultMocks([target]);
      mocks.bridges.NRWorkspacesRestoreBackup = () => {
        mocks.bridges.NRWorkspacesListBackups = () => ok([preRestore, target]);
        return ok({ success: true, skippedCount: 2 });
      };
      await withPage(mocks, async (view) => {
        await until(() => view.text().includes("3 tabs"), "list rendered");
        const restoreButton = view.buttons().find((b) =>
          b.textContent?.trim() === "Restore"
        )!;
        await view.click(restoreButton);
        await until(
          () =>
            document.body.textContent?.includes("Restore workspace backup") ??
              false,
          "workspaces confirm dialog opens",
        );
        const dialog = document.querySelector("[role=alertdialog]");
        assert(dialog, "dialog uses role=alertdialog");
        const confirm = [...dialog!.querySelectorAll("button")].find((b) =>
          b.textContent?.trim() === "Restore"
        );
        assert(confirm, "dialog has a Restore confirm button");
        await view.click(confirm!);
        await until(
          () => view.text().includes("Pre-restore"),
          "the refreshed list shows the pre-restore backup",
        );
        const restoreCall = mocks.calls.find((c) =>
          c.name === "NRWorkspacesRestoreBackup"
        );
        assertEquals(restoreCall?.args[0], target.fileName, "restored file");
        assert(
          view.text().includes(
            "2 tabs were skipped because their address can't be restored safely.",
          ),
          "skipped count is reported",
        );
        assert(
          view.text().includes(
            "Last backup: " + dateText("en-US", preRestore.capturedAt),
          ),
          "last backup updated",
        );
      });
    },
  },
  {
    name: "V-02 plural forms for tab, window and workspace counts",
    fn: async () => {
      const backups = [
        summary({ id: "a1", fileName: "a1", tabCount: 1, workspaceCount: 1 }),
        summary({ id: "a2", fileName: "a2", tabCount: 2, workspaceCount: 2 }),
        summary({ id: "a3", fileName: "a3", mode: "tabs", windowCount: 1 }),
        summary({ id: "a4", fileName: "a4", mode: "tabs", windowCount: 2 }),
      ];
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("2 tabs"), "list rendered");
        const desc = (id: string) =>
          view.host.querySelector(`#backup-desc-${id}`)?.textContent ?? "";
        assert(desc("a1").startsWith("1 workspace · 1 tab ·"), desc("a1"));
        assert(desc("a2").startsWith("2 workspaces · 2 tabs ·"), desc("a2"));
        assert(desc("a3").startsWith("1 window · 3 tabs ·"), desc("a3"));
        assert(desc("a4").startsWith("2 windows · 3 tabs ·"), desc("a4"));
      });
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("個のタブ"), "ja list rendered");
        const text = view.host.querySelector("#backup-desc-a1")?.textContent;
        assert(
          text?.startsWith("1 個のワークスペース · 1 個のタブ"),
          `ja: ${text}`,
        );
      }, "ja-JP");
    },
  },
  {
    name: "V-04 restore/delete buttons have aria-labels with kind and time",
    fn: async () => {
      const backups = [
        summary({ kind: "auto" }),
        summary({
          id: "pr000001",
          fileName: "pr",
          kind: "pre-restore",
        }),
      ];
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("3 tabs"), "list rendered");
        const time = dateText("en-US");
        for (
          const [kind, id] of [["Auto", "abcdef01"], [
            "Pre-restore",
            "pr000001",
          ]]
        ) {
          for (const verb of ["Restore", "Delete"]) {
            const label = `${verb} the ${kind} backup from ${time}`;
            const found = view.buttons().find((b) =>
              b.getAttribute("aria-label") === label
            );
            assert(found, `aria-label "${label}"`);
            assertEquals(
              found!.getAttribute("aria-describedby"),
              `backup-desc-${id}`,
              "button is described by the row details",
            );
          }
        }
      });
      await withPage(defaultMocks(backups), async (view) => {
        await until(() => view.text().includes("個のタブ"), "ja list rendered");
        const label = `${dateText("ja-JP")} の自動バックアップを復元`;
        assert(
          view.buttons().some((b) => b.getAttribute("aria-label") === label),
          `ja aria-label "${label}"`,
        );
      }, "ja-JP");
    },
  },
  {
    name:
      "V-03 controls are keyboard reachable and dialogs focus Cancel and close on Escape",
    fn: async () => {
      await withPage(defaultMocks([summary()]), async (view) => {
        await until(() => view.text().includes("3 tabs"), "list rendered");
        for (const b of view.buttons()) {
          assert(b.tabIndex >= 0, `button "${b.textContent}" is tabbable`);
        }
        for (
          const id of ["backup-enabled", "backup-interval", "backup-retention"]
        ) {
          const control = view.host.querySelector<HTMLElement>(`#${id}`);
          assert(control && control.tabIndex >= 0, `#${id} is tabbable`);
          assert(
            view.host.querySelector(`label[for=${id}]`),
            `#${id} has a label`,
          );
        }
        const opener = view.buttons().find((b) =>
          b.textContent?.trim() === "Delete"
        )!;
        opener.focus();
        await view.click(opener);
        await until(
          () => Boolean(document.querySelector("[role=alertdialog]")),
          "delete dialog opens",
        );
        await until(
          () => document.activeElement?.textContent?.trim() === "Cancel",
          "initial focus is on Cancel",
        );
        await act(async () => {
          document.activeElement?.dispatchEvent(
            new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
          );
          await Promise.resolve();
        });
        await until(
          () => !document.querySelector("[role=alertdialog]"),
          "Escape closes the dialog",
        );
        await until(
          () => document.activeElement === opener,
          "focus returns to the opener",
        );
      });
    },
  },
  {
    name: "V-01 no raw translation keys are rendered in any settings locale",
    fn: async () => {
      const keyPattern = /\b(?:backup|workspaces|ui|pages)\.[A-Za-z]+\b/;
      for (const path of Object.keys(LOCALES)) {
        const lng = path.match(/locales\/(.*)\.json$/)![1];
        // "ja-JP-mac" is a Mozilla-only file name, not a BCP 47 tag; the
        // runtime language is "ja-JP-macos" (or "ja-JP"), so it is never used.
        if (lng === "ja-JP-mac") continue;
        const mocks = defaultMocks([
          summary(),
          summary({ id: "t1", fileName: "t1", mode: "tabs", kind: "auto" }),
        ]);
        mocks.bridges.NRWorkspacesBackupNow = () => fail("restore-in-progress");
        await withPage(mocks, async (view) => {
          await until(
            () => view.host.querySelectorAll("li").length === 2,
            `${lng} list`,
          );
          await view.click(view.button(view.i18n.t("workspaces.backupNow")));
          await until(
            () => Boolean(view.host.querySelector("[role=alert]")),
            `${lng} action error`,
          );
          const texts = [
            view.text(),
            ...view.buttons().map((b) => b.getAttribute("aria-label") ?? ""),
            ...[...view.host.querySelectorAll("option")].map((o) =>
              o.textContent ?? ""
            ),
          ].join("\n");
          const match = texts.match(keyPattern);
          assert(!match, `${lng}: raw key "${match?.[0]}" is rendered`);
          assert(!texts.includes("{{"), `${lng}: unreplaced interpolation`);
        }, lng);
      }
    },
  },
  {
    name: "U-03 the page renders 100 backups in a practical time",
    fn: async () => {
      const backups = Array.from({ length: 100 }, (_, i) =>
        summary({
          id: `u03${i.toString(16).padStart(5, "0")}`,
          fileName: `backup-u03-${i}`,
          capturedAt: CAPTURED_AT - i * 60_000,
          kind: i % 3 === 0 ? "auto" : i % 3 === 1 ? "manual" : "pre-restore",
        }));
      const start = performance.now();
      await withPage(defaultMocks(backups), async (view) => {
        await until(
          () => view.host.querySelectorAll("li").length === 100,
          "100 rows rendered",
        );
        const elapsed = performance.now() - start;
        assert(elapsed < 2000, `rendering 100 rows took ${elapsed} ms`);
        assertEquals(
          view.buttons().filter((b) => b.getAttribute("aria-describedby"))
            .length,
          200,
          "restore and delete per row",
        );
      });
    },
  },
  {
    name:
      "P-03 search for バックアップ / backup finds the three backup settings",
    fn: async () => {
      for (
        const [lng, query] of [["ja-JP", "バックアップ"], ["en-US", "backup"]]
      ) {
        const i18n = await createI18n(lng);
        const documents = buildSearchDocuments(i18n);
        const needle = normalizeSearchText(query);
        const fields = documents.filter((d) =>
          d.route.startsWith("/features/backup?setting=")
        );
        assertEquals(fields.length, 3, `${lng}: three backup fields`);
        for (const field of fields) {
          assert(
            field.normalizedTitle.includes(needle) ||
              field.normalizedText.includes(needle),
            `${lng}: ${field.id} matches "${query}"`,
          );
        }
        assertEquals(
          JSON.stringify(fields.map((f) => f.route).sort()),
          JSON.stringify([
            "/features/backup?setting=backup-enabled",
            "/features/backup?setting=backup-interval",
            "/features/backup?setting=backup-retention",
          ]),
          `${lng}: routes point at the backup page`,
        );
        const section = documents.find((d) => d.id === "backup");
        assert(
          section?.normalizedText.includes(needle),
          `${lng}: section match`,
        );
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("workspacesBackupQaG8SettingsUi.test.ts", tests);
}
