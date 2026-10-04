// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  config,
  setConfig,
  updateChromeExtrasSetting,
} from "../../designs/configs.ts";
import { firefoxSidebarOverlay } from "../index.ts";
import type { NativeSidebarController } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const native =
  (globalThis as unknown as { SidebarController: NativeSidebarController })
    .SidebarController;

async function waitFor(check: () => boolean, message: string): Promise<void> {
  const deadline = Date.now() + 10_000;
  while (!check()) {
    assert(Date.now() < deadline, message);
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

async function settle(): Promise<void> {
  await native.waitUntilStable();
  for (let i = 0; i < 3; i++) {
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
  }
}

function element(id: string): HTMLElement {
  const result = document.getElementById(id);
  assert(result, `${id} must exist`);
  return result;
}

function hover(inside: boolean): void {
  element("floorp-firefox-sidebar-edge-toggle").dispatchEvent(
    new MouseEvent(inside ? "mouseenter" : "mouseleave"),
  );
}

function focusContent(): void {
  (globalThis as unknown as {
    gBrowser: { selectedBrowser: { focus(): void } };
  }).gBrowser.selectedBrowser.focus();
}

function viewport(): DOMRect {
  const selected =
    (globalThis as unknown as { gBrowser: { selectedBrowser: HTMLElement } })
      .gBrowser.selectedBrowser;
  return selected.getBoundingClientRect();
}

function assertViewport(before: DOMRect, message: string): void {
  const after = viewport();
  assert(
    Math.abs(before.width - after.width) < 1,
    `${message}: width ${before.width} -> ${after.width}`,
  );
  assert(
    Math.abs(before.height - after.height) < 1,
    `${message}: height ${before.height} -> ${after.height}`,
  );
}

function assertSidebarContentWidth(message: string): void {
  const browser = element("sidebar");
  const stack = browser.parentElement;
  assert(stack, "native sidebar browser must have a layout parent");
  const width = browser.getBoundingClientRect().width;
  const parentWidth = stack.getBoundingClientRect().width;
  const panelWidth = element("sidebar-box").getBoundingClientRect().width;
  assert(
    Math.abs(width - parentWidth) < 1,
    `${message}: embedded document must fill its parent (${width}/${parentWidth})`,
  );
  assert(
    width >= panelWidth - 24,
    `${message}: embedded document must follow panel width (${width}/${panelWidth})`,
  );
}

async function setModes(overlay: boolean, hoverMode: boolean): Promise<void> {
  updateChromeExtrasSetting("sidebarOverlap", overlay);
  updateChromeExtrasSetting("autohideSidebar", hoverMode);
  await waitFor(
    () =>
      document.documentElement.hasAttribute(
          "floorp-firefox-sidebar-overlay",
        ) === overlay &&
      document.documentElement.hasAttribute("floorp-firefox-sidebar-hover") ===
        (overlay && hoverMode),
    "sidebar settings must reach chrome",
  );
  await settle();
}

async function withSidebar(test: () => Promise<void>): Promise<void> {
  await native.promiseInitialized;
  await waitFor(
    () => Boolean(firefoxSidebarOverlay),
    "Firefox sidebar feature must initialize",
  );
  const before = config();
  const originalID = native.currentID;
  const originalLast = native.lastOpenedId;
  const wasOpen = native.isOpen;
  const originalWidth = element("sidebar-box").style.width;
  const prefs = ["sidebar.position_start", "sidebar.visibility"];
  const savedPrefs = prefs.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: name === "sidebar.position_start"
      ? Services.prefs.getBoolPref(name, true)
      : Services.prefs.getStringPref(name, "always-show"),
  }));
  try {
    await setModes(false, false);
    native.hide({ dismissPanel: false });
    await settle();
    await test();
  } finally {
    hover(false);
    focusContent();
    document.documentElement.removeAttribute("inDOMFullscreen");
    native.hide({ dismissPanel: false });
    setConfig(before);
    for (const pref of savedPrefs) {
      if (!pref.user) Services.prefs.clearUserPref(pref.name);
      else if (typeof pref.value === "boolean") {
        Services.prefs.setBoolPref(pref.name, pref.value);
      } else Services.prefs.setStringPref(pref.name, pref.value);
    }
    native.setPosition();
    element("sidebar-box").style.width = originalWidth;
    native.lastOpenedId = originalLast;
    await settle();
    if (wasOpen && native.sidebars.has(originalID)) {
      await native.showInitially(originalID);
    }
    await settle();
  }
}

async function testOverlayGeometry(): Promise<void> {
  await withSidebar(async () => {
    for (const visibility of ["always-show", "hide-launcher"]) {
      Services.prefs.setStringPref("sidebar.visibility", visibility);
      await settle();
      for (const start of [true, false]) {
        await setModes(false, false);
        native.hide({ dismissPanel: false });
        Services.prefs.setBoolPref("sidebar.position_start", start);
        native.setPosition();
        await settle();
        await setModes(true, false);
        const before = viewport();
        await native.showInitially("viewBookmarksSidebar");
        await settle();
        assertViewport(
          before,
          `native overlay ${visibility} at ${start ? "start" : "end"}`,
        );
        assertSidebarContentWidth("native overlay opening");
        const panel = element("sidebar-box").getBoundingClientRect();
        const content = element("tabbrowser-tabbox").getBoundingClientRect();
        assert(
          panel.left >= content.left - 1 && panel.right <= content.right + 1,
          "overlay panel must fit within web content",
        );
        const resizer = element("floorp-firefox-sidebar-overlay-resizer");
        resizer.dispatchEvent(
          new KeyboardEvent("keydown", {
            key: start ? "ArrowRight" : "ArrowLeft",
            bubbles: true,
          }),
        );
        await settle();
        assert(
          element("sidebar-box").getBoundingClientRect().width > panel.width,
          "overlay panel must remain resizable",
        );
        assertViewport(before, "overlay resize must preserve website geometry");
        assertSidebarContentWidth("native overlay resize");
        native.hide({ dismissPanel: false });
        await settle();
        assertViewport(before, "native close must preserve website geometry");
      }
    }
  });
}

async function testHoverAndPin(): Promise<void> {
  await withSidebar(async () => {
    native.lastOpenedId = "viewBookmarksSidebar";
    await setModes(true, true);
    const before = viewport();
    const active = document.activeElement;
    hover(true);
    await waitFor(
      () => native.isOpen && Boolean(firefoxSidebarOverlay?.expanded),
      "edge hover must open a closed native sidebar",
    );
    await settle();
    assertEquals(
      native.currentID,
      "viewBookmarksSidebar",
      "hover should restore the remembered panel",
    );
    assertEquals(
      document.activeElement,
      active,
      "hover must not steal keyboard focus",
    );
    assertViewport(before, "hover opening must preserve website geometry");
    assertSidebarContentWidth("native hover opening");
    const width = element("sidebar-box").getBoundingClientRect().width;
    hover(false);
    focusContent();
    await waitFor(
      () => !firefoxSidebarOverlay?.expanded,
      "leaving the native panel must collapse it",
    );
    assert(native.isOpen, "hover collapse must preserve the selected document");
    assertEquals(
      element("sidebar-box").getBoundingClientRect().width,
      width,
      "hover collapse must preserve embedded document width",
    );
    assertViewport(before, "hover collapse must preserve website geometry");
    assertSidebarContentWidth("native hover collapse");

    hover(true);
    await waitFor(
      () => Boolean(firefoxSidebarOverlay?.expanded),
      "edge hover must reveal a collapsed document",
    );
    element("floorp-firefox-sidebar-edge-toggle").dispatchEvent(
      new MouseEvent("click", { bubbles: true, detail: 1 }),
    );
    assert(firefoxSidebarOverlay?.pinned, "click must pin the panel open");
    hover(false);
    focusContent();
    await new Promise((resolve) => setTimeout(resolve, 450));
    assert(
      firefoxSidebarOverlay?.expanded,
      "pinned panel must stay open after leaving",
    );
    assertSidebarContentWidth("native pin after the pointer leaves");
    const trigger = element("floorp-firefox-sidebar-edge-toggle");
    trigger.focus();
    trigger.dispatchEvent(
      new MouseEvent("click", { bubbles: true, detail: 1 }),
    );
    await waitFor(
      () => !firefoxSidebarOverlay?.expanded,
      "pointer unpin must release trigger focus and collapse after leaving",
    );
    trigger.focus();
    await waitFor(
      () => Boolean(firefoxSidebarOverlay?.expanded),
      "keyboard focus must reveal the panel",
    );
    trigger.dispatchEvent(
      new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
    );
    assert(
      !firefoxSidebarOverlay?.expanded,
      "Escape must collapse a keyboard-open panel",
    );
    assertViewport(
      before,
      "pin and keyboard interactions must preserve website geometry",
    );
    await native.showInitially(native.currentID);
    assert(
      firefoxSidebarOverlay?.expanded && firefoxSidebarOverlay.pinned,
      "a native command for an already loaded collapsed panel must reveal and pin it",
    );
  });
}

async function testRetentionAndFullscreen(): Promise<void> {
  await withSidebar(async () => {
    await setModes(true, true);
    hover(true);
    await waitFor(
      () => Boolean(firefoxSidebarOverlay?.expanded),
      "panel must open for retention checks",
    );
    const popup = document.createXULElement("menupopup");
    document.documentElement.appendChild(popup);
    try {
      popup.dispatchEvent(new Event("popupshown", { bubbles: true }));
      hover(false);
      focusContent();
      await new Promise((resolve) => setTimeout(resolve, 450));
      assert(
        firefoxSidebarOverlay?.expanded,
        "an open popup must retain the native panel",
      );
      popup.dispatchEvent(new Event("popuphidden", { bubbles: true }));
      await waitFor(
        () => !firefoxSidebarOverlay?.expanded,
        "closing the popup must reconcile hover state",
      );
    } finally {
      popup.remove();
    }
    native.hide({ dismissPanel: false });
    document.documentElement.setAttribute("inDOMFullscreen", "true");
    await settle();
    const trigger = element("floorp-firefox-sidebar-edge-toggle");
    trigger.dispatchEvent(new Event("focusin", { bubbles: true }));
    trigger.dispatchEvent(new MouseEvent("click", { bubbles: true }));
    hover(true);
    await new Promise((resolve) => setTimeout(resolve, 250));
    assert(
      !native.isOpen && !firefoxSidebarOverlay?.expanded,
      "fullscreen must suppress all hover, click and focus activation paths",
    );
    assertEquals(
      getComputedStyle(trigger)?.display,
      "none",
      "fullscreen must hide the edge control",
    );
  });
}

async function testPinnedSidebarRestoresAfterFullscreen(): Promise<void> {
  await withSidebar(async () => {
    await setModes(true, true);
    hover(true);
    await waitFor(
      () => native.isOpen && Boolean(firefoxSidebarOverlay?.expanded),
      "native sidebar must open before pinning",
    );
    element("floorp-firefox-sidebar-edge-toggle").dispatchEvent(
      new MouseEvent("click", { bubbles: true, detail: 1 }),
    );
    hover(false);
    focusContent();
    assert(
      firefoxSidebarOverlay?.pinned,
      "native sidebar must be pinned before entering fullscreen",
    );
    const before = viewport();
    document.documentElement.setAttribute("inDOMFullscreen", "true");
    await settle();
    assert(
      firefoxSidebarOverlay.pinned && !firefoxSidebarOverlay.expanded,
      "fullscreen must hide the panel while preserving its pin",
    );
    assertEquals(
      getComputedStyle(element("sidebar-box"))?.visibility,
      "collapse",
      "fullscreen must hide pinned native content",
    );
    document.documentElement.removeAttribute("inDOMFullscreen");
    await settle();
    assert(
      native.isOpen && firefoxSidebarOverlay.pinned &&
        firefoxSidebarOverlay.expanded,
      "leaving fullscreen must restore a pinned native panel",
    );
    assertViewport(
      before,
      "restoring a pinned native panel must preserve the website viewport",
    );
    assertSidebarContentWidth("native pinned panel after fullscreen");
  });
}

async function testPendingOpenInvalidation(): Promise<void> {
  await withSidebar(async () => {
    const originalShow = native.showInitially;
    let release: (() => void) | undefined;
    try {
      for (const earlyReveal of [true, false]) {
        let requested = false;
        let completed = false;
        native.showInitially = async (command) => {
          requested = true;
          // Gecko unhides the panel synchronously before waiting for its load.
          const early = earlyReveal
            ? originalShow.call(native, command)
            : undefined;
          await new Promise<void>((resolve) => release = resolve);
          const shown = await (early ?? originalShow.call(native, command));
          completed = true;
          return shown;
        };
        await setModes(true, true);
        hover(true);
        await waitFor(
          () => requested,
          "hover must start the delayed native open",
        );
        await settle();
        if (earlyReveal) {
          assert(
            native.isOpen,
            "native panel must be visible while its load is pending",
          );
        }
        const before = viewport();
        await setModes(false, false);
        assert(
          !native.isOpen,
          "disabling overlay must immediately hide a hover-owned pending panel",
        );
        assertViewport(
          before,
          "disabling overlay during native loading must not briefly dock the panel",
        );
        release?.();
        await waitFor(
          () => completed && !native.isOpen,
          "stale native opening must not create a docked sidebar",
        );
        assert(
          !native.isOpen,
          "a completed obsolete hover request must stay closed",
        );
        hover(false);
      }

      let completed = false;
      native.showInitially = async (command) => {
        const shown = originalShow.call(native, command);
        await new Promise<void>((resolve) => release = resolve);
        completed = true;
        return shown;
      };
      await setModes(true, true);
      hover(true);
      await waitFor(
        () => native.isOpen,
        "native close regression requires an in-flight visible panel",
      );
      native.hide({ dismissPanel: false });
      await settle();
      assert(
        !firefoxSidebarOverlay?.expanded,
        "native close must cancel hover even during loading",
      );
      release?.();
      await waitFor(
        () => completed && !native.isOpen,
        "native close must survive the old load completion",
      );
    } finally {
      release?.();
      native.showInitially = originalShow;
    }
  });
}

async function testFailedOpenWaitsForNewGesture(): Promise<void> {
  await withSidebar(async () => {
    const originalShow = native.showInitially;
    const originalError = console.error;
    const expected = new Error("expected native sidebar opening failure");
    let attempts = 0;
    try {
      console.error = (...args: unknown[]) => {
        if (args[0] !== "[FirefoxSidebar]" || args[1] !== expected) {
          originalError.apply(console, args);
        }
      };
      native.showInitially = () => {
        attempts++;
        return Promise.reject(expected);
      };
      await setModes(true, true);
      hover(true);
      await waitFor(
        () => attempts === 1,
        "hover must attempt to open the native sidebar",
      );
      await new Promise((resolve) => setTimeout(resolve, 450));
      assertEquals(
        attempts,
        1,
        "a failed opening must wait for a new gesture instead of retrying continuously",
      );
      hover(false);
      hover(true);
      await waitFor(
        () => attempts === 2,
        "a new hover gesture must allow another opening attempt",
      );
    } finally {
      native.showInitially = originalShow;
      console.error = originalError;
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("sidebarOverlay.test.ts", [
    {
      name: "native overlay and resize preserve the viewport on both sides",
      fn: testOverlayGeometry,
    },
    {
      name:
        "native edge hover, pin and keyboard control preserve the selected document",
      fn: testHoverAndPin,
    },
    {
      name:
        "popups retain native hover panels and fullscreen suppresses activation",
      fn: testRetentionAndFullscreen,
    },
    {
      name: "obsolete asynchronous native hover opening is cancelled",
      fn: testPendingOpenInvalidation,
    },
    {
      name: "pinned native sidebar survives DOM fullscreen",
      fn: testPinnedSidebarRestoresAfterFullscreen,
    },
    {
      name: "failed native hover opening waits for a new gesture",
      fn: testFailedOpenWaitsForNewGesture,
    },
  ]);
}
