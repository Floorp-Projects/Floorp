// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CHROME_EXTRAS_SIDEBAR_HEADER_PREF } from "../chrome-extras.ts";
import {
  config,
  getChromeExtrasSettings,
  setConfig,
  updateChromeExtrasSetting,
} from "../configs.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

type LitHeader = HTMLElement & { updateComplete: Promise<boolean> };
type SidebarTestController = {
  promiseInitialized: Promise<void>;
  browser: { contentDocument: Document };
  currentID: string;
  isOpen: boolean;
  sidebars: Map<string, { url: string }>;
  show(id: string): Promise<void>;
  hide(): void;
};
declare const SidebarController: SidebarTestController;

async function waitFor(check: () => boolean, message: string): Promise<void> {
  const deadline = Date.now() + 10_000;
  while (!check()) {
    assert(Date.now() < deadline, message);
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

function display(element: Element): string {
  const win = element.ownerDocument.defaultView;
  assert(win, "element must have a window");
  return win.getComputedStyle(element).display;
}

async function testEveryDesignMirrorsOnlyTheSavedToggle(): Promise<void> {
  const before = config();
  try {
    // Seed a stale mirror, as after a crash or a change from another window.
    Services.prefs.setBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, true);
    for (
      const design of [
        "lepton",
        "photon",
        "protonfix",
        "proton",
        "fluerial",
      ] as const
    ) {
      setConfig((prev) => ({
        ...prev,
        globalConfigs: { ...prev.globalConfigs, userInterface: design },
        uiCustomization: {
          ...prev.uiCustomization,
          chromeExtras: {
            ...getChromeExtrasSettings(),
            hiddenSidebarHeader: false,
          },
        },
      }));
      await waitFor(
        () =>
          !Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, true),
        `${design}: saved false must overwrite a stale mirror`,
      );
      updateChromeExtrasSetting("hiddenSidebarHeader", true);
      await waitFor(
        () =>
          Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, false),
        `${design}: enabling the toggle must update the Runtime mirror`,
      );
    }
  } finally {
    setConfig(before);
    await waitFor(
      () =>
        Services.prefs.getBoolPref(CHROME_EXTRAS_SIDEBAR_HEADER_PREF, false) ===
          getChromeExtrasSettings().hiddenSidebarHeader,
      "restoring the config must restore the derived mirror",
    );
  }
}

async function testRevampedHeadersKeepSearchAndCollapseEmptyHosts(): Promise<
  void
> {
  const before = config();
  const pref = "sidebar.revamp";
  const hadUserPref = Services.prefs.prefHasUserValue(pref);
  const previousPref = Services.prefs.getBoolPref(pref, false);
  const previousID = SidebarController.currentID;
  const wasOpen = SidebarController.isOpen;
  let probe: LitHeader | null = null;
  try {
    await SidebarController.promiseInitialized;
    Services.prefs.setBoolPref(pref, true);
    await waitFor(
      () =>
        SidebarController.sidebars.get("viewHistorySidebar")?.url.endsWith(
          "sidebar-history.html",
        ) === true,
      "revamped History must become available without changing the tab bar",
    );
    updateChromeExtrasSetting("hiddenSidebarHeader", false);
    await SidebarController.show("viewHistorySidebar");
    await waitFor(
      () =>
        !!SidebarController.browser.contentDocument.querySelector(
          "sidebar-history",
        )?.shadowRoot?.querySelector("sidebar-panel-header")?.shadowRoot,
      "the real History panel and its header must finish rendering",
    );
    const doc = SidebarController.browser.contentDocument;
    const page = doc.querySelector("sidebar-history");
    const header = page?.shadowRoot?.querySelector<LitHeader>(
      "sidebar-panel-header",
    );
    assert(
      header,
      "History must use the Firefox sidebar-panel-header component",
    );
    await header.updateComplete;
    const heading = header.shadowRoot?.querySelector(".sidebar-panel-heading");
    const search = header.querySelector("moz-input-search");
    const options = header.querySelector(".menu-button");
    assert(
      heading && search && options,
      "History heading, search and options must exist",
    );
    await waitFor(() => display(heading) === "flex", "header CSS must load");
    assertEquals(
      display(heading),
      "flex",
      "the default keeps the heading visible",
    );

    // webext-panels.xhtml has an empty light-DOM host of this same component.
    // Exercise its real class/CSS without installing a test extension.
    probe = doc.createElement("sidebar-panel-header") as LitHeader;
    probe.setAttribute("heading", "Extension header probe");
    probe.setAttribute("view", "viewHistorySidebar");
    header.parentElement?.appendChild(probe);
    await probe.updateComplete;
    await waitFor(
      () => probe !== null && probe.getBoundingClientRect().height > 0,
      "empty host must initially be visible",
    );

    updateChromeExtrasSetting("hiddenSidebarHeader", true);
    await waitFor(
      () => display(heading) === "none",
      "the toggle must reach the child document and shadow root",
    );
    assert(
      search.getBoundingClientRect().height > 0,
      "hiding the title must keep the slotted search usable",
    );
    assert(
      options.getBoundingClientRect().height > 0,
      "History's options button must remain visible",
    );
    assertEquals(
      display(probe),
      "none",
      "an empty extension-style host must leave no header space",
    );

    updateChromeExtrasSetting("hiddenSidebarHeader", false);
    await waitFor(
      () => display(heading) === "flex",
      "turning the toggle off must restore the same heading",
    );
    assert(
      probe.getBoundingClientRect().height > 0,
      "turning the toggle off must restore an empty host",
    );
    probe.remove();
    probe = null;

    // A newly loaded panel must inherit the enabled setting too.
    updateChromeExtrasSetting("hiddenSidebarHeader", true);
    SidebarController.hide();
    await SidebarController.show("viewHistorySidebar");
    await waitFor(() => {
      const reloaded = SidebarController.browser.contentDocument.querySelector(
        "sidebar-history",
      )?.shadowRoot?.querySelector("sidebar-panel-header")?.shadowRoot
        ?.querySelector(".sidebar-panel-heading");
      return !!reloaded && display(reloaded) === "none";
    }, "a reopened panel must inherit the enabled toggle");
  } finally {
    probe?.remove();
    SidebarController.hide();
    setConfig(before);
    if (hadUserPref) Services.prefs.setBoolPref(pref, previousPref);
    else Services.prefs.clearUserPref(pref);
    await new Promise((resolve) => setTimeout(resolve, 0));
    if (wasOpen && previousID && SidebarController.sidebars.has(previousID)) {
      await SidebarController.show(previousID);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("sidebar-header.test.ts", [
    {
      name: "every design mirrors the canonical toggle",
      fn: testEveryDesignMirrorsOnlyTheSavedToggle,
    },
    {
      name: "revamped headers hide only their title row",
      fn: testRevampedHeadersKeepSearchAndCollapseEmptyHosts,
    },
  ]);
}

await runAllTests();
