// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act, lazy, Suspense, useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { createInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import {
  MemoryRouter,
  Route,
  Routes,
  useLocation,
  useNavigate,
} from "react-router-dom";
import type { AvailableExperiment } from "../../../modules/common/defines.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  CONTEXT_MENU_SETTINGS_ROUTE,
  ContextMenuAvailabilityGate,
  ContextMenuAvailabilityProvider,
  useContextMenuAvailability,
} from "../../src/lib/experiments/context-menu-availability.tsx";
import {
  EXPERIMENTS_POLICY_PREF,
  experimentsRpc,
  notifyExperimentsChanged,
} from "../../src/lib/rpc/experiments.ts";
import { rpc } from "../../src/lib/rpc/rpc.ts";
import { AppSidebar } from "../../src/components/app-sidebar.tsx";
import SearchPage from "../../src/app/search/page.tsx";

function enrolled(
  overrides: Partial<AvailableExperiment> = {},
): AvailableExperiment {
  return {
    id: "context_menu_customization",
    name: "Context menus",
    description: undefined,
    rollout: 0,
    start: undefined,
    end: undefined,
    isActive: true,
    enrollmentStatus: "enrolled",
    currentVariantId: "enabled",
    experimentData: { id: "context_menu_customization" },
    ...overrides,
  };
}

async function flushReact(): Promise<void> {
  await act(async () => {
    await Promise.resolve();
    await Promise.resolve();
  });
}

function stubRpc(
  readExperiments: () => Promise<AvailableExperiment[]>,
  readPolicy: () => Promise<string | null> = () => Promise.resolve("default"),
) {
  const originalExperiments = experimentsRpc.getAllExperiments;
  const originalStringPref = rpc.getStringPref;
  const originalBoolPref = rpc.getBoolPref;
  let reads = 0;
  experimentsRpc.getAllExperiments = () => {
    reads++;
    return readExperiments();
  };
  rpc.getStringPref = (name) => {
    assertEquals(
      name,
      EXPERIMENTS_POLICY_PREF,
      "only participation policy is read",
    );
    return readPolicy();
  };
  rpc.getBoolPref = () => Promise.resolve(true);
  return {
    reads: () => reads,
    restore: () => {
      experimentsRpc.getAllExperiments = originalExperiments;
      rpc.getStringPref = originalStringPref;
      rpc.getBoolPref = originalBoolPref;
    },
  };
}

async function renderAvailability(initialRoute: string) {
  const i18n = createInstance();
  await i18n.init({
    lng: "en",
    fallbackLng: false,
    initImmediate: false,
    interpolation: { escapeValue: false },
    resources: {
      en: {
        translation: {
          pages: { contextMenu: "Context menus", home: "Home" },
          contextMenu: { description: "Arrange browser context menus" },
        },
      },
    },
  });
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  let pathname = "";
  let availability = "loading";
  let navigate: ReturnType<typeof useNavigate> | null = null;
  let lazyLoads = 0;
  let editorMounts = 0;
  function Editor() {
    const [selection, setSelection] = useState("Default profile");
    useEffect(() => {
      editorMounts++;
    }, []);
    return (
      <button
        type="button"
        data-editor="true"
        onClick={() => setSelection("Selected profile")}
      >
        {selection}
      </button>
    );
  }
  const LazyEditor = lazy(() => {
    lazyLoads++;
    return Promise.resolve({ default: Editor });
  });
  function Probe() {
    pathname = useLocation().pathname;
    availability = useContextMenuAvailability();
    navigate = useNavigate();
    return null;
  }
  await act(async () => {
    root.render(
      <I18nextProvider i18n={i18n}>
        <MemoryRouter initialEntries={[initialRoute]}>
          <ContextMenuAvailabilityProvider>
            <Probe />
            <div data-sidebar="true">
              <AppSidebar />
            </div>
            <main>
              <Suspense fallback={<p>Loading route</p>}>
                <Routes>
                  <Route path="/search" element={<SearchPage />} />
                  <Route
                    path={CONTEXT_MENU_SETTINGS_ROUTE}
                    element={
                      <ContextMenuAvailabilityGate>
                        <LazyEditor />
                      </ContextMenuAvailabilityGate>
                    }
                  />
                  <Route path="*" element={<div>Home</div>} />
                </Routes>
              </Suspense>
            </main>
          </ContextMenuAvailabilityProvider>
        </MemoryRouter>
      </I18nextProvider>,
    );
    await Promise.resolve();
  });
  await flushReact();
  const linkSelector = `a[href="${CONTEXT_MENU_SETTINGS_ROUTE}"]`;
  return {
    host,
    pathname: () => pathname,
    availability: () => availability,
    lazyLoads: () => lazyLoads,
    editorMounts: () => editorMounts,
    sidebarVisible: () =>
      Boolean(host.querySelector(`[data-sidebar] ${linkSelector}`)),
    searchVisible: () => Boolean(host.querySelector(`main ${linkSelector}`)),
    navigate: async (route: string) => {
      assert(navigate !== null, "the router has mounted");
      await act(async () => await navigate!(route));
      await flushReact();
    },
    changed: async () => {
      await act(async () => {
        notifyExperimentsChanged();
        await Promise.resolve();
      });
      await flushReact();
    },
    focus: async () => {
      await act(async () => {
        globalThis.dispatchEvent(new Event("focus"));
        await Promise.resolve();
      });
      await flushReact();
    },
    cleanup: () => {
      act(() => root.unmount());
      host.remove();
    },
  };
}

const tests: TestCase[] = [
  {
    name:
      "Focus and same-route query changes preserve an approved editor's local state",
    fn: async () => {
      const pending = Promise.withResolvers<AvailableExperiment[]>();
      let next = Promise.resolve([enrolled()]);
      const stub = stubRpc(() => next);
      const view = await renderAvailability(CONTEXT_MENU_SETTINGS_ROUTE);
      try {
        const editor = view.host.querySelector<HTMLButtonElement>(
          "[data-editor]",
        );
        assert(editor, "approved editor is mounted");
        act(() => editor.click());
        await view.navigate(`${CONTEXT_MENU_SETTINGS_ROUTE}?setting=example`);
        assertEquals(
          stub.reads(),
          1,
          "query-only navigation does not reload Flasco",
        );
        next = pending.promise;
        await view.focus();
        assertEquals(
          view.availability(),
          "available",
          "focus checks in the background",
        );
        assertEquals(
          view.host.querySelector("[data-editor]"),
          editor,
          "the pending check retains the editor",
        );
        pending.resolve([enrolled()]);
        await flushReact();
        assertEquals(
          view.editorMounts(),
          1,
          "successful focus does not remount the editor",
        );
        assertEquals(
          editor.textContent,
          "Selected profile",
          "selected context survives native menu interaction",
        );
        next = Promise.resolve([]);
        await view.focus();
        assertEquals(
          view.pathname(),
          "/overview/home",
          "a revoked background result redirects",
        );
        assert(
          !view.host.querySelector("[data-editor]"),
          "revoked editor unmounts",
        );
      } finally {
        view.cleanup();
        stub.restore();
      }
    },
  },
  {
    name:
      "A pending Flasco check never loads the lazy editor or exposes navigation",
    fn: async () => {
      const pending = Promise.withResolvers<AvailableExperiment[]>();
      const stub = stubRpc(() => pending.promise);
      const view = await renderAvailability(CONTEXT_MENU_SETTINGS_ROUTE);
      try {
        assertEquals(
          view.availability(),
          "loading",
          "initial state is pending",
        );
        assertEquals(
          view.lazyLoads(),
          0,
          "lazy module is untouched while pending",
        );
        assert(!view.sidebarVisible(), "sidebar hides the pending feature");
        assert(
          view.host.querySelector('[role="status"]'),
          "loading is announced",
        );
        pending.resolve([enrolled()]);
        await flushReact();
        assertEquals(
          view.lazyLoads(),
          1,
          "approved enrollment loads the editor",
        );
        assert(
          view.host.querySelector("[data-editor]"),
          "approved editor mounts",
        );
        assert(view.sidebarVisible(), "the same approval exposes navigation");
        assertEquals(
          stub.reads(),
          1,
          "consumers share one experiments request",
        );
      } finally {
        view.cleanup();
        stub.restore();
      }
    },
  },
  {
    name:
      "Missing, inactive, disabled, control, unknown variant and opted-out Flascos deny direct URLs",
    fn: async () => {
      const cases = [
        { rows: [], policy: "default" },
        { rows: [enrolled({ id: "different_experiment" })], policy: "default" },
        { rows: [enrolled({ isActive: false })], policy: "default" },
        {
          rows: [enrolled({ enrollmentStatus: "disabled" })],
          policy: "default",
        },
        {
          rows: [enrolled({ enrollmentStatus: "control" })],
          policy: "default",
        },
        {
          rows: [enrolled({ enrollmentStatus: "not_in_rollout" })],
          policy: "default",
        },
        {
          rows: [enrolled({ currentVariantId: "control" })],
          policy: "default",
        },
        { rows: [enrolled({ currentVariantId: "other" })], policy: "always" },
        {
          rows: [
            enrolled({
              currentVariantId: null,
              enrollmentStatus: "force_enrolled",
            }),
          ],
          policy: "default",
        },
        {
          rows: [enrolled({ enrollmentStatus: "force_enrolled" })],
          policy: "never",
        },
      ];
      for (const { rows, policy } of cases) {
        const stub = stubRpc(
          () => Promise.resolve(rows),
          () => Promise.resolve(policy),
        );
        const view = await renderAvailability(CONTEXT_MENU_SETTINGS_ROUTE);
        try {
          assertEquals(
            view.pathname(),
            "/overview/home",
            "denied URLs redirect home",
          );
          assertEquals(
            view.lazyLoads(),
            0,
            "denied URLs never load the editor",
          );
          assert(!view.sidebarVisible(), "denied entries remain hidden");
          await view.navigate("/search?q=context");
          assert(
            !view.searchVisible(),
            "denied settings are absent from search",
          );
        } finally {
          view.cleanup();
          stub.restore();
        }
      }
    },
  },
  {
    name:
      "Mutation events update sidebar and search together without remounting the Hub",
    fn: async () => {
      let rows: AvailableExperiment[] = [];
      let policy: string | null = null;
      const stub = stubRpc(
        () => Promise.resolve(rows),
        () => Promise.resolve(policy),
      );
      const view = await renderAvailability("/search?q=context");
      try {
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "absent Flasco is hidden everywhere",
        );
        rows = [enrolled({ enrollmentStatus: "force_enrolled" })];
        await view.changed();
        assert(
          view.sidebarVisible() && view.searchVisible(),
          "force enrollment updates both consumers",
        );
        assertEquals(
          stub.reads(),
          2,
          "the notification causes one shared refresh",
        );
        policy = "never";
        await view.changed();
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "policy opt-out hides stale cached enrollment",
        );
        policy = "default";
        await view.changed();
        assert(
          view.sidebarVisible() && view.searchVisible(),
          "the next valid snapshot restores the entries",
        );
        rows = [];
        await view.changed();
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "cleared assignments remove both entries",
        );
      } finally {
        view.cleanup();
        stub.restore();
      }
    },
  },
  {
    name:
      "A new route waits for a fresh check instead of mounting with previous permission",
    fn: async () => {
      const pending = Promise.withResolvers<AvailableExperiment[]>();
      let next = Promise.resolve([enrolled()]);
      const stub = stubRpc(() => next);
      const view = await renderAvailability("/search?q=context");
      try {
        assert(view.searchVisible(), "initial enrollment is available");
        next = pending.promise;
        await view.navigate(CONTEXT_MENU_SETTINGS_ROUTE);
        assertEquals(
          view.availability(),
          "loading",
          "route change refreshes approval",
        );
        assertEquals(
          view.lazyLoads(),
          0,
          "previous permission cannot load the new route",
        );
        next = Promise.resolve([]);
        pending.resolve([]);
        await flushReact();
        assertEquals(
          view.pathname(),
          "/overview/home",
          "revoked access redirects",
        );
        assertEquals(view.lazyLoads(), 0, "revoked editor was never loaded");
      } finally {
        view.cleanup();
        stub.restore();
      }
    },
  },
  {
    name:
      "Focus refreshes discard stale async responses and remove listeners on unmount",
    fn: async () => {
      const old = Promise.withResolvers<AvailableExperiment[]>();
      const current = Promise.withResolvers<AvailableExperiment[]>();
      let next = old.promise;
      const stub = stubRpc(() => next);
      const view = await renderAvailability("/search?q=context");
      try {
        next = current.promise;
        await view.focus();
        current.resolve([]);
        await flushReact();
        old.resolve([enrolled()]);
        await flushReact();
        assertEquals(
          view.availability(),
          "unavailable",
          "older success cannot overwrite the latest denial",
        );
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "stale approval stays hidden",
        );
        next = Promise.resolve([enrolled()]);
        await view.focus();
        assert(
          view.sidebarVisible() && view.searchVisible(),
          "focus accepts a current approval",
        );
        view.cleanup();
        const count = stub.reads();
        globalThis.dispatchEvent(new Event("focus"));
        notifyExperimentsChanged();
        await flushReact();
        assertEquals(
          stub.reads(),
          count,
          "unmounted provider has no active listeners",
        );
      } finally {
        if (view.host.isConnected) view.cleanup();
        stub.restore();
      }
    },
  },
  {
    name: "Either RPC read failing revokes a previously available feature",
    fn: async () => {
      let failExperiments = false;
      let failPolicy = false;
      const stub = stubRpc(
        () =>
          failExperiments
            ? Promise.reject(new Error("experiments unavailable"))
            : Promise.resolve([enrolled()]),
        () =>
          failPolicy
            ? Promise.reject(new Error("policy unavailable"))
            : Promise.resolve("default"),
      );
      const view = await renderAvailability("/search?q=context");
      try {
        assert(view.searchVisible(), "feature starts available");
        failPolicy = true;
        await view.changed();
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "policy read failure denies access",
        );
        failPolicy = false;
        await view.changed();
        assert(view.searchVisible(), "successful retry restores availability");
        failExperiments = true;
        await view.changed();
        assert(
          !view.sidebarVisible() && !view.searchVisible(),
          "experiment read failure denies access",
        );
      } finally {
        view.cleanup();
        stub.restore();
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("context-menu-availability.test.tsx", tests);
}
