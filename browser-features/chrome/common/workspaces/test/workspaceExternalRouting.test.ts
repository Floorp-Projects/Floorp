// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../index.ts";
import { enabled, setEnabled } from "../data/config.ts";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { getWorkspaceSessionIdentityService } from "../utils/workspace-sessions.ts";
import { getWorkspaceSessionRuntime } from "../utils/workspace-session-runtime.ts";
import { waitFor, withWindows } from "./workspace-last-tab-test-utils.ts";
import type { RoutingWindow } from "../../../../modules/modules/WorkspaceExternalRouting.types.mts";
import type { WorkspaceExternalRoutingService } from "../../../../modules/modules/WorkspaceExternalRouting.sys.mts";
import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const { WorkspaceExternalRouting: routing } = ChromeUtils.importESModule(
  "resource://noraneko/modules/WorkspaceExternalRouting.sys.mjs",
) as { WorkspaceExternalRouting: WorkspaceExternalRoutingService };
const principal = () => Services.scriptSecurityManager.getSystemPrincipal();
const identities = getWorkspaceSessionIdentityService();

function setContext(id: TWorkspaceID, userContextId: number): void {
  const definition = Workspaces.getCtx()?.getRawWorkspace(id);
  assert(definition, "workspace definition exists");
  setWorkspacesDataStore("data", (previous) => {
    const definitions = new Map(
      previous as unknown as Map<TWorkspaceID, TWorkspace>,
    );
    definitions.set(id, { ...definition, userContextId });
    return definitions as unknown as TWorkspacesStoreData["data"];
  });
}

async function withRouting(
  check: (
    source: TWorkspaceID,
    target: TWorkspaceID,
    context: number,
  ) => void | Promise<void>,
): Promise<void> {
  await waitFor(() => Boolean(Workspaces.getCtx()), "workspaces initialized");
  const service = Workspaces.getCtx();
  assert(service, "workspace service exists");
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalEnabled = enabled();
  const originalWorkspace = service.getSelectedWorkspaceID();
  const originalTab = globalThis.gBrowser.selectedTab;
  const originalTabs = new Set(globalThis.gBrowser.tabs);
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const diverted = Services.prefs.getBoolPref(
    "browser.tabs.loadDivertedInBackground",
    false,
  );
  const hadDiverted = Services.prefs.prefHasUserValue(
    "browser.tabs.loadDivertedInBackground",
  );
  const identity = identities.create(
    "External routing test",
    "fingerprint",
    "green",
  );
  try {
    setEnabled(true);
    Services.prefs.setBoolPref("browser.tabs.loadDivertedInBackground", false);
    const source = service.createNoNameWorkspace();
    const target = service.createNoNameWorkspace();
    setContext(source, identity.userContextId);
    service.changeWorkspace(source);
    await check(source, target, identity.userContextId);
  } finally {
    setEnabled(true);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    for (const id of [...workspacesDataStore.order]) {
      if (!originalWorkspaces.has(id)) service.deleteWorkspace(id);
    }
    for (const tab of [...globalThis.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) globalThis.gBrowser.removeTab(tab);
    }
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    identities.remove(identity.userContextId);
    setEnabled(originalEnabled);
    if (hadDiverted) {
      Services.prefs.setBoolPref(
        "browser.tabs.loadDivertedInBackground",
        diverted,
      );
    } else {Services.prefs.clearUserPref(
        "browser.tabs.loadDivertedInBackground",
      );}
  }
}

function openExternal(where: number): unknown {
  const domWindow = Reflect.get(window, "browserDOMWindow") as unknown as {
    openURI(
      uri: nsIURI,
      opener: null,
      where: number,
      flags: number,
      principal: nsIPrincipal,
    ): unknown;
  };
  return domWindow.openURI(
    Services.io.newURI("about:blank"),
    null,
    where,
    Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL!,
    principal(),
  );
}

const tests: TestCase[] = [
  {
    name:
      "External current-tab requests reuse only a matching ready actual session",
    async fn() {
      await withRouting((_source, _target, context) => {
        const original = globalThis.gBrowser.selectedTab;
        assertEquals(
          Number(original.getAttribute("usercontextid") || 0),
          0,
          "workspace's existing standard tab is preserved",
        );
        const count = globalThis.gBrowser.tabs.length;
        openExternal(Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW!);
        assertEquals(
          globalThis.gBrowser.tabs.length,
          count + 1,
          "context mismatch opens a new browser",
        );
        const matching = globalThis.gBrowser.selectedTab;
        assertEquals(
          Number(matching.linkedBrowser?.getAttribute("usercontextid")),
          context,
          "first browser uses receiver workspace default",
        );
        // Simulate a legacy tab whose display attribute was changed after its
        // browser was created. Actual browsing-context identity remains decisive.
        matching.setAttribute("usercontextid", "0");
        try {
          openExternal(Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW!);
          assertEquals(
            globalThis.gBrowser.tabs.length,
            count + 1,
            "matching actual context permits reuse despite a stale display attribute",
          );
        } finally {
          matching.setAttribute("usercontextid", String(context));
        }
        matching.setAttribute("pending", "true");
        try {
          openExternal(Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW!);
          assertEquals(
            globalThis.gBrowser.tabs.length,
            count + 2,
            "pending tab is never reused",
          );
        } finally {
          matching.removeAttribute("pending");
        }
      });
    },
  },
  {
    name:
      "Deferred external requests retain window, workspace, context and background selection",
    async fn() {
      await withRouting(async (source, target, context) => {
        const service = Workspaces.getCtx()!;
        const request = routing.capture(window);
        service.changeWorkspace(target);
        const selected = globalThis.gBrowser.selectedTab;
        const before = new Set(globalThis.gBrowser.tabs);
        routing.withRequest(
          window,
          request,
          () => openExternal(Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW!),
        );
        const opened = [...globalThis.gBrowser.tabs].find((tab) =>
          !before.has(tab)
        );
        assert(opened, "external request creates one background tab");
        assertEquals(
          opened.getAttribute("floorpWorkspaceId"),
          source,
          "captured workspace owns the new tab before TabOpen",
        );
        assertEquals(
          Number(opened.linkedBrowser?.getAttribute("usercontextid")),
          context,
          "captured context does not follow later selection",
        );
        assert(opened.hidden, "other workspace's new tab stays hidden");
        assertEquals(
          service.getSelectedWorkspaceID(),
          target,
          "selected workspace is unchanged",
        );
        assertEquals(
          globalThis.gBrowser.selectedTab,
          selected,
          "later selected tab is unchanged",
        );
        await withWindows(false, false, async (open) => {
          const other = await open();
          assertEquals(
            routing.canReuse(other, request),
            false,
            "another window cannot consume captured reuse target",
          );
          assertThrows(
            () => routing.withRequest(other, request, () => null),
            "captured delivery cannot be rebound to another window",
          );
        });
      });
    },
  },
  {
    name:
      "Deleted captured workspaces hold the request until the user explicitly selects another destination",
    async fn() {
      await withRouting((source, target) => {
        const service = Workspaces.getCtx()!;
        const request = routing.capture(window);
        service.changeWorkspace(target);
        service.deleteWorkspace(source);
        assert(
          !service.isWorkspaceID(source),
          "captured destination was removed",
        );
        const box = Reflect.get(window, "gNotificationBox") as NonNullable<
          RoutingWindow["gNotificationBox"]
        >;
        const append = box.appendNotification;
        let notification = "";
        let openHere: (() => void) | null = null;
        box.appendNotification = (_id, options, buttons) => {
          notification = options.label;
          openHere = buttons[0]?.callback ?? null;
          return Promise.resolve(null);
        };
        try {
          const selected = globalThis.gBrowser.selectedTab;
          const count = globalThis.gBrowser.tabs.length;
          routing.withRequest(
            window,
            request,
            () => openExternal(Ci.nsIBrowserDOMWindow.OPEN_CURRENTWINDOW!),
          );
          assertEquals(
            globalThis.gBrowser.tabs.length,
            count,
            "no browser or network request is created in another workspace",
          );
          assertEquals(
            globalThis.gBrowser.selectedTab,
            selected,
            "newer user selection remains intact",
          );
          assert(
            notification && openHere,
            "nonblocking notification retains a manual open action",
          );
          selected.setAttribute("pending", "true");
          try {
            (openHere as (() => void) | null)?.();
          } finally {
            selected.removeAttribute("pending");
          }
          assertEquals(
            globalThis.gBrowser.tabs.length,
            count + 1,
            "a new explicit destination can open the held link",
          );
        } finally {
          box.appendNotification = append;
        }
      });
    },
  },
  {
    name:
      "Fresh normal windows use default workspace before the first web browser and honor explicit standard",
    async fn() {
      await withRouting(async (source, target, context) => {
        setWorkspacesDataStore("defaultID", source);
        Workspaces.getCtx()!.changeWorkspace(target);
        await withWindows(false, false, async (open) => {
          const other = await open();
          assertEquals(
            other.gBrowser.selectedTab.getAttribute("floorpWorkspaceId"),
            source,
            "new window starts in the global default workspace",
          );
          assertEquals(
            Number(
              other.gBrowser.selectedTab.linkedBrowser?.getAttribute(
                "usercontextid",
              ),
            ),
            context,
            "initial browser is created in default workspace context",
          );
        });
        const newWindow = (window as unknown as {
          openDialog(
            url: string,
            name: string,
            features: string,
            ...args: unknown[]
          ): Window;
        }).openDialog(
          "chrome://browser/content/browser.xhtml",
          "_blank",
          "chrome,all,dialog=no,non-private",
          "about:blank",
          null,
          null,
          null,
          false,
          0,
        );
        try {
          await waitFor(
            () =>
              Boolean(
                Reflect.get(newWindow, "gBrowserInit")?.delayedStartupFinished,
              ),
            "explicit-standard window finishes startup",
            30_000,
          );
          const browser = Reflect.get(
            newWindow,
            "gBrowser",
          ) as typeof globalThis.gBrowser;
          assertEquals(
            Number(
              browser.selectedTab.linkedBrowser?.getAttribute("usercontextid"),
            ),
            0,
            "explicit initial context 0 wins over default session",
          );
        } finally {
          newWindow.close();
        }
      });
    },
  },
  {
    name:
      "A missing initial session holds the requested URL in a blank placeholder",
    async fn() {
      await withRouting(async (source) => {
        setWorkspacesDataStore("defaultID", source);
        setContext(source, 2_147_483_600);
        await withWindows(false, false, async (open) => {
          const other = await open();
          const tab = other.gBrowser.selectedTab;
          assertEquals(
            tab.linkedBrowser?.currentURI?.spec,
            "about:blank",
            "initial URL never loads in standard",
          );
          assertEquals(
            Number(tab.linkedBrowser?.getAttribute("usercontextid")),
            0,
            "only blank placeholder uses context 0",
          );
          assertEquals(
            getWorkspaceSessionRuntime().getPendingState(tab)?.userContextId,
            2_147_483_600,
            "missing initial context remains recoverable",
          );
        });
      });
    },
  },
];

export async function runAllTests(): Promise<void> {
  const privateRequest = routing.capture(null, true);
  assertEquals(
    privateRequest.workspaceId,
    null,
    "pending private window does not capture a normal workspace",
  );
  assertEquals(
    privateRequest.userContextId,
    0,
    "pending private window uses standard context",
  );
  await runTests("workspaceExternalRouting.test.ts", tests);
}
