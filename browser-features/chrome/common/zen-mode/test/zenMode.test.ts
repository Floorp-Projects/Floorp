// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "solid-js";
import { render } from "@nora/solid-xul";
import {
  attachZenModeToWindow,
  destroyZenModeForWindow,
  getZenModeController,
  toggleZenModeForWindow,
  ZEN_MODE_HIDE_DELAY_MS,
  ZEN_MODE_PREF,
  ZEN_MODE_STYLE_ID,
  ZenModeMenuElement,
} from "../zen-mode.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type TestWindowBundle = {
  win: Window;
  doc: Document;
  toolbox: Element;
  dispatchWindowEvent: (event: Event) => boolean;
};

function makeRect(
  width: number,
  height: number,
  left = 0,
  top = 0,
): DOMRect {
  return {
    x: left,
    y: top,
    width,
    height,
    left,
    top,
    right: left + width,
    bottom: top + height,
    toJSON: () => ({ width, height, left, top }),
  } as DOMRect;
}

function setRect(
  element: Element,
  width: number,
  height: number,
  left = 0,
  top = 0,
): void {
  Object.defineProperty(element, "getBoundingClientRect", {
    configurable: true,
    value: () => makeRect(width, height, left, top),
  });
}

function createTestWindow(): TestWindowBundle {
  const doc = document.implementation.createHTMLDocument("Zen test window");
  const windowEvents = new EventTarget();
  const toolbox = doc.createElement("div");
  toolbox.id = "navigator-toolbox";
  setRect(toolbox, 1000, 100, 0, 0);
  doc.body!.appendChild(toolbox);

  const hostWindow = window;
  const win = {
    document: doc,
    closed: false,
    innerWidth: 1000,
    innerHeight: 800,
    MutationObserver,
    ResizeObserver,
    requestAnimationFrame: hostWindow.requestAnimationFrame.bind(hostWindow),
    cancelAnimationFrame: hostWindow.cancelAnimationFrame.bind(hostWindow),
    setTimeout: hostWindow.setTimeout.bind(hostWindow),
    clearTimeout: hostWindow.clearTimeout.bind(hostWindow),
    addEventListener: windowEvents.addEventListener.bind(windowEvents),
    removeEventListener: windowEvents.removeEventListener.bind(windowEvents),
    gNavToolbox: toolbox,
  } as unknown as Window;

  return {
    win,
    doc,
    toolbox,
    dispatchWindowEvent: (event) => windowEvents.dispatchEvent(event),
  };
}

async function withSeed<T>(
  seed: boolean,
  callback: () => T | Promise<T>,
): Promise<T> {
  const hadUserValue = Services.prefs.prefHasUserValue(ZEN_MODE_PREF);
  const original = Services.prefs.getBoolPref(ZEN_MODE_PREF, false);
  Services.prefs.setBoolPref(ZEN_MODE_PREF, seed);
  try {
    return await callback();
  } finally {
    if (hadUserValue) {
      Services.prefs.setBoolPref(ZEN_MODE_PREF, original);
    } else {
      Services.prefs.clearUserPref(ZEN_MODE_PREF);
    }
  }
}

function wait(ms: number): Promise<void> {
  return new Promise((resolve) => globalThis.setTimeout(resolve, ms));
}

const tests: TestCase[] = [
  {
    name: "one controller and one owned stylesheet are created per window",
    async fn() {
      await withSeed(false, () => {
        const bundle = createTestWindow();
        const first = attachZenModeToWindow(bundle.win);
        const second = attachZenModeToWindow(bundle.win);
        assert(first !== null, "first controller should be created");
        assertEquals(second, first, "the window registry should be idempotent");
        assertEquals(
          bundle.doc.querySelectorAll(`#${ZEN_MODE_STYLE_ID}`).length,
          1,
          "the window should own exactly one Zen stylesheet",
        );

        const css = bundle.doc.getElementById(ZEN_MODE_STYLE_ID)?.textContent ??
          "";
        assert(
          css.includes("translateY(-100%)"),
          "toolbox retraction should use its own current height",
        );
        assert(
          css.includes("translateY(100%)"),
          "statusbar retraction should use its own current height",
        );
        assert(
          css.includes("prefers-reduced-motion: reduce"),
          "the owned stylesheet should honor reduced motion",
        );

        destroyZenModeForWindow(bundle.win);
        assertEquals(
          bundle.doc.getElementById(ZEN_MODE_STYLE_ID),
          null,
          "destroy should remove the owned stylesheet",
        );
      });
    },
  },
  {
    name:
      "existing controllers ignore external pref writes and new ones snapshot them",
    async fn() {
      await withSeed(false, () => {
        const firstWindow = createTestWindow();
        const first = attachZenModeToWindow(firstWindow.win);
        assert(first !== null, "first controller should be created");
        assertEquals(first.enabled(), false, "first window should seed false");

        Services.prefs.setBoolPref(ZEN_MODE_PREF, true);
        assertEquals(
          first.enabled(),
          false,
          "external writes must not mutate an existing controller",
        );

        const secondWindow = createTestWindow();
        const second = attachZenModeToWindow(secondWindow.win);
        assert(second !== null, "second controller should be created");
        assertEquals(second.enabled(), true, "new window should seed true");

        Services.prefs.setBoolPref(ZEN_MODE_PREF, false);
        assertEquals(
          first.enabled(),
          false,
          "first window should remain false",
        );
        assertEquals(
          second.enabled(),
          true,
          "second window should remain true",
        );

        destroyZenModeForWindow(firstWindow.win);
        destroyZenModeForWindow(secondWindow.win);
      });
    },
  },
  {
    name: "explicit target-window toggles persist without changing peers",
    async fn() {
      await withSeed(false, () => {
        const firstWindow = createTestWindow();
        const secondWindow = createTestWindow();
        const first = attachZenModeToWindow(firstWindow.win);
        const second = attachZenModeToWindow(secondWindow.win);
        assert(first !== null && second !== null, "controllers should exist");

        assertEquals(toggleZenModeForWindow(secondWindow.win), true, "toggle");
        assertEquals(
          first.enabled(),
          false,
          "untargeted window must not change",
        );
        assertEquals(second.enabled(), true, "targeted window should change");
        assertEquals(
          Services.prefs.getBoolPref(ZEN_MODE_PREF, false),
          true,
          "explicit toggle should persist the future-window seed",
        );

        destroyZenModeForWindow(firstWindow.win);
        destroyZenModeForWindow(secondWindow.win);
      });
    },
  },
  {
    name: "owning gNavToolbox customization disables locally without restore",
    async fn() {
      await withSeed(true, () => {
        const bundle = createTestWindow();
        const controller = attachZenModeToWindow(bundle.win);
        assert(controller !== null, "controller should exist");
        assertEquals(controller.enabled(), true, "window should seed enabled");

        bundle.toolbox.dispatchEvent(new Event("customizationstarting"));
        assertEquals(
          controller.enabled(),
          false,
          "customization should disable only this controller",
        );
        assertEquals(
          Services.prefs.getBoolPref(ZEN_MODE_PREF, false),
          true,
          "customization must not persist the temporary disable",
        );

        bundle.toolbox.dispatchEvent(new Event("aftercustomization"));
        assertEquals(
          controller.enabled(),
          false,
          "leaving customization must not auto-restore Zen",
        );

        destroyZenModeForWindow(bundle.win);
      });
    },
  },
  {
    name: "late sidebar mounts use only positive rendered measurements",
    async fn() {
      await withSeed(true, async () => {
        const bundle = createTestWindow();
        const controller = attachZenModeToWindow(bundle.win);
        assert(controller !== null, "controller should exist");

        const sidebar = bundle.doc.createElement("div");
        sidebar.id = "panel-sidebar-box";
        setRect(sidebar, 284, 600);
        bundle.doc.body!.appendChild(sidebar);

        const selectBox = bundle.doc.createElement("div");
        selectBox.id = "panel-sidebar-select-box";
        setRect(selectBox, 44, 600);
        bundle.doc.body!.appendChild(selectBox);

        const statusbar = bundle.doc.createElement("div");
        statusbar.id = "nora-statusbar";
        setRect(statusbar, 1000, 32, 0, 768);
        bundle.doc.body!.appendChild(statusbar);

        await wait(0);
        const root = bundle.doc.documentElement as HTMLElement;
        assertEquals(
          root.style.getPropertyValue("--zenmode-sidebar-width"),
          "284px",
          "late sidebar should establish a rendered width",
        );
        assertEquals(
          root.style.getPropertyValue("--zenmode-selectbox-width"),
          "44px",
          "late select box should establish a rendered width",
        );

        setRect(sidebar, 0, 0);
        sidebar.remove();
        bundle.doc.body!.appendChild(sidebar);
        await wait(0);
        assertEquals(
          root.style.getPropertyValue("--zenmode-sidebar-width"),
          "284px",
          "a zero-sized render must not poison the last positive width",
        );

        const css = bundle.doc.getElementById(ZEN_MODE_STYLE_ID)?.textContent ??
          "";
        assert(
          css.includes(":root[zenmode] #nora-statusbar"),
          "late statusbar should already be covered by owned Zen CSS",
        );

        destroyZenModeForWindow(bundle.win);
      });
    },
  },
  {
    name: "sibling mainPopupSet app-menu keeps the owning window revealed",
    async fn() {
      await withSeed(true, async () => {
        const bundle = createTestWindow();
        const popupSet = bundle.doc.createElement("div");
        popupSet.id = "mainPopupSet";
        const appMenu = bundle.doc.createElement("panel");
        appMenu.setAttribute("open", "true");
        popupSet.appendChild(appMenu);
        bundle.doc.body!.appendChild(popupSet);

        const controller = attachZenModeToWindow(bundle.win);
        assert(controller !== null, "controller should exist");

        bundle.dispatchWindowEvent(
          new MouseEvent("mousemove", { clientX: 100, clientY: 5 }),
        );
        bundle.dispatchWindowEvent(
          new MouseEvent("mousemove", { clientX: 100, clientY: 200 }),
        );
        await wait(ZEN_MODE_HIDE_DELAY_MS + 60);
        assert(
          bundle.doc.documentElement!.hasAttribute("zenmode-reveal-top"),
          "an app-menu in the owning document should hold the reveal open",
        );

        appMenu.removeAttribute("open");
        await wait(ZEN_MODE_HIDE_DELAY_MS + 60);
        assert(
          !bundle.doc.documentElement!.hasAttribute("zenmode-reveal-top"),
          "closing the owning app-menu should allow the reveal to retract",
        );

        destroyZenModeForWindow(bundle.win);
      });
    },
  },
  {
    name: "late urlbar focus and open state preserve the top reveal",
    async fn() {
      await withSeed(true, async () => {
        const bundle = createTestWindow();
        const controller = attachZenModeToWindow(bundle.win);
        assert(controller !== null, "controller should exist");

        const urlbar = bundle.doc.createElement("div");
        urlbar.id = "urlbar";
        bundle.doc.body!.appendChild(urlbar);
        urlbar.dispatchEvent(new FocusEvent("focusin", { bubbles: true }));
        assert(
          bundle.doc.documentElement!.hasAttribute("zenmode-reveal-top"),
          "a late-mounted urlbar should reveal on focus",
        );

        urlbar.setAttribute("open", "true");
        urlbar.dispatchEvent(new FocusEvent("focusout", { bubbles: true }));
        await wait(ZEN_MODE_HIDE_DELAY_MS + 60);
        assert(
          bundle.doc.documentElement!.hasAttribute("zenmode-reveal-top"),
          "an open urlbar view should hold the reveal open",
        );

        urlbar.removeAttribute("open");
        await wait(ZEN_MODE_HIDE_DELAY_MS + 60);
        assert(
          !bundle.doc.documentElement!.hasAttribute("zenmode-reveal-top"),
          "closing the urlbar view should allow retraction",
        );

        destroyZenModeForWindow(bundle.win);
      });
    },
  },
  {
    name:
      "destroy removes attributes variables observers listeners and registry state",
    async fn() {
      await withSeed(false, () => {
        const bundle = createTestWindow();
        const controller = attachZenModeToWindow(bundle.win);
        assert(controller !== null, "controller should exist");
        controller.setEnabledFromUser(true);

        const root = bundle.doc.documentElement as HTMLElement;
        root.setAttribute("zenmode-reveal-top", "");
        root.setAttribute("zenmode-reveal-bottom", "");
        root.setAttribute("zenmode-reveal-side", "");
        root.setAttribute("zenmode-rebase", "");
        root.style.setProperty("--zenmode-sidebar-width", "300px");
        root.style.setProperty("--zenmode-statusbar-height", "30px");

        destroyZenModeForWindow(bundle.win);
        for (
          const attribute of [
            "zenmode",
            "zenmode-reveal-top",
            "zenmode-reveal-bottom",
            "zenmode-reveal-side",
            "zenmode-rebase",
          ]
        ) {
          assert(
            !root.hasAttribute(attribute),
            `destroy should remove ${attribute}`,
          );
        }
        for (
          const property of [
            "--zenmode-toolbox-height",
            "--zenmode-sidebar-width",
            "--zenmode-selectbox-width",
            "--zenmode-statusbar-height",
          ]
        ) {
          assertEquals(
            root.style.getPropertyValue(property),
            "",
            `destroy should remove ${property}`,
          );
        }
        assertEquals(
          getZenModeController(bundle.win),
          undefined,
          "destroy should remove the registry entry",
        );
        assertEquals(
          (bundle.win as unknown as { __floorpZenModeController?: unknown })
            .__floorpZenModeController,
          undefined,
          "destroy should remove the cross-context window marker",
        );
        assertEquals(
          bundle.doc.getElementById(ZEN_MODE_STYLE_ID),
          null,
          "destroy should remove Zen CSS",
        );

        bundle.dispatchWindowEvent(
          new MouseEvent("mousemove", { clientX: 100, clientY: 5 }),
        );
        bundle.toolbox.dispatchEvent(new Event("customizationstarting"));
        assert(
          !root.hasAttribute("zenmode-reveal-top"),
          "destroyed listeners must not mutate the document",
        );
      });
    },
  },
  {
    name: "menu reflects and toggles its owning window controller",
    async fn() {
      await withSeed(false, () => {
        const controller = attachZenModeToWindow(window);
        assert(controller !== null, "real window controller should exist");
        const originalEnabled = controller.enabled();
        controller.setEnabledFromUser(false);

        const container = document.createElement("div");
        document.body?.appendChild(container);
        const dispose = createRoot((rootDispose) => {
          render(
            () => ZenModeMenuElement({ targetWindow: window }),
            container,
          );
          return rootDispose;
        });

        try {
          const menuitem = container.querySelector("#toggle_zenmode");
          assert(menuitem !== null, "menu item should render");
          assertEquals(
            menuitem.getAttribute("accesskey"),
            "Z",
            "menu item should retain its access key",
          );
          menuitem.dispatchEvent(new Event("command", { bubbles: true }));
          assertEquals(
            controller.enabled(),
            true,
            "menu command should toggle its owning window",
          );
          assert(
            menuitem.hasAttribute("checked"),
            "menu checked state should react to its controller",
          );
        } finally {
          dispose();
          container.remove();
          controller.setEnabledFromUser(originalEnabled);
        }
      });
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("zenMode.test.ts", tests);
}
