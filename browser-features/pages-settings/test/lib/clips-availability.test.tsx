// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { act, lazy, Suspense } from "react";
import { createRoot } from "react-dom/client";
import { createInstance } from "i18next";
import { I18nextProvider } from "react-i18next";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  CLIPS_SETTINGS_ROUTE,
  ClipsAvailabilityGate,
  ClipsAvailabilityProvider,
} from "../../src/lib/experiments/clips-availability.tsx";
import { rpc } from "../../src/lib/rpc/rpc.ts";
import { AppSidebar } from "../../src/components/app-sidebar.tsx";
import SearchPage from "../../src/app/search/page.tsx";

async function checkAvailability(
  enabled: boolean,
  fail = false,
): Promise<void> {
  const original = rpc.getBoolPref;
  let resolve: (value: boolean) => void = () => {};
  let reject: (reason: Error) => void = () => {};
  const gate = new Promise<boolean>((accept, refuse) => {
    resolve = accept;
    reject = refuse;
  });
  rpc.getBoolPref = (name) =>
    name === "floorp.browser.clips.enabled" ? gate : Promise.resolve(true);
  const i18n = createInstance();
  await i18n.init({
    lng: "en",
    fallbackLng: false,
    initImmediate: false,
    resources: {
      en: {
        translation: {
          pages: { clips: "Clips" },
          clips: { description: "Clipboard history settings" },
        },
      },
    },
  });
  const host = document.createElement("div");
  document.body.append(host);
  const root = createRoot(host);
  let loaded = 0;
  const Page = lazy(() => {
    loaded++;
    return Promise.resolve({
      default: () => <div data-clips-settings="true" />,
    });
  });
  try {
    await act(async () => {
      root.render(
        <I18nextProvider i18n={i18n}>
          <MemoryRouter initialEntries={["/features/clips?q=clipboard"]}>
            <ClipsAvailabilityProvider>
              <aside data-navigation="true">
                <AppSidebar />
              </aside>
              <div data-search="true">
                <SearchPage />
              </div>
              <Suspense fallback={<p>Loading route</p>}>
                <Routes>
                  <Route
                    path={CLIPS_SETTINGS_ROUTE}
                    element={
                      <ClipsAvailabilityGate>
                        <Page />
                      </ClipsAvailabilityGate>
                    }
                  />
                  <Route path="*" element={<div data-home="true" />} />
                </Routes>
              </Suspense>
            </ClipsAvailabilityProvider>
          </MemoryRouter>
        </I18nextProvider>,
      );
      await Promise.resolve();
    });
    assertEquals(loaded, 0, "a pending gate does not load the lazy page");
    assertEquals(
      host.querySelectorAll('a[href^="/features/clips"]').length,
      0,
      "navigation and search hide Clips while the gate is pending",
    );
    await act(async () => {
      if (fail) reject(new Error("startup gate unavailable"));
      else resolve(enabled);
      await gate.catch(() => {});
    });
    assertEquals(
      loaded,
      enabled ? 1 : 0,
      "only an enabled gate loads the page",
    );
    for (const surface of ["navigation", "search"]) {
      assertEquals(
        Boolean(
          host.querySelector(`[data-${surface}] a[href^="/features/clips"]`),
        ),
        enabled,
        `${surface} follows the same gate as the route`,
      );
    }
    assert(
      host.querySelector(enabled ? "[data-clips-settings]" : "[data-home]"),
      "the direct route renders Clips or redirects home",
    );
  } finally {
    await act(async () => {
      root.unmount();
      await Promise.resolve();
    });
    host.remove();
    rpc.getBoolPref = original;
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "enabled Clips appears on every settings surface",
      fn: () => checkAvailability(true),
    },
    {
      name: "disabled Clips stays out of lazy routes and search",
      fn: () => checkAvailability(false),
    },
    {
      name: "a failed gate read keeps Clips unavailable",
      fn: () => checkAvailability(false, true),
    },
  ];
  await runTests("clips-availability.test.tsx", tests);
}
