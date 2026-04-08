// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { render } from "@nora/solid-xul";
import { assert, runTests } from "../../../test/utils/test_harness.ts";
import { HubPanelMenu } from "../HubPanelMenu.tsx";

const PANEL_UI_BUTTON_ID = "PanelUI-menu-button";
const SETTINGS_BUTTON_ID = "appMenu-settings-button";
const MAIN_VIEW_ID = "appMenu-mainView";
const PANEL_BODY_CLASS = "panel-subview-body";
const HUB_BUTTON_ID = "appMenu-floorp-hub-button";

function cleanupDOM(): void {
  document!.getElementById(PANEL_UI_BUTTON_ID)?.remove();
  document!.getElementById(SETTINGS_BUTTON_ID)?.remove();
  document!.getElementById(MAIN_VIEW_ID)?.remove();
  document!.getElementById(HUB_BUTTON_ID)?.remove();
}

function createMockPanelMenu(): HTMLElement {
  cleanupDOM();

  const panelUIButton = document!.createElement("div");
  panelUIButton.id = PANEL_UI_BUTTON_ID;
  document!.body!.appendChild(panelUIButton);

  const settingsButton = document!.createElement("div");
  settingsButton.id = SETTINGS_BUTTON_ID;
  document!.body!.appendChild(settingsButton);

  const afterSettings = document!.createElement("div");
  settingsButton.after(afterSettings);

  const mainView = document!.createElement("div");
  mainView.id = MAIN_VIEW_ID;
  const panelBody = document!.createElement("div");
  panelBody.className = PANEL_BODY_CLASS;
  mainView.appendChild(panelBody);
  document!.body!.appendChild(mainView);

  return panelUIButton;
}

function testHubPanelMenuExports(): void {
  assert(
    typeof HubPanelMenu === "function",
    "HubPanelMenu should be a constructor function",
  );
}

function testHubPanelMenuDoesNotThrowWithoutPanelUIButton(): void {
  cleanupDOM();
  let threw = false;
  try {
    new HubPanelMenu();
  } catch {
    threw = true;
  }
  assert(
    !threw,
    "HubPanelMenu constructor should not throw when UI elements are missing",
  );
}

function testHubPanelMenuConstructsWithPanelButton(): void {
  cleanupDOM();
  createMockPanelMenu();

  let threw = false;
  try {
    new HubPanelMenu();
  } catch {
    threw = true;
  }

  assert(
    !threw,
    "HubPanelMenu constructor should not throw when panel UI elements are available",
  );

  cleanupDOM();
}

function testStaticRenderCreatesToolbarButton(): void {
  const container = document!.createElement("div");
  document!.body!.appendChild(container);

  render(() => HubPanelMenu.Render(), container);

  const rendered = container.querySelector(`#${HUB_BUTTON_ID}`);
  assert(
    rendered !== null,
    "HubPanelMenu.Render should create a toolbarbutton element with the expected ID",
  );

  container.remove();
}

function testHandleOpenHubAddsTabAndHidesPanel(): void {
  // deno-lint-ignore no-explicit-any
  const originalPanelUI = (globalThis as any).PanelUI;
  // deno-lint-ignore no-explicit-any
  const originalGBrowser = (globalThis as any).gBrowser;

  let hideCalled = false;
  let addTabCalled = false;

  if (originalPanelUI && typeof originalPanelUI === "object") {
    // deno-lint-ignore no-explicit-any
    (originalPanelUI as any).hide = () => {
      hideCalled = true;
    };
  } else {
    Object.defineProperty(globalThis, "PanelUI", {
      value: {
        hide: () => {
          hideCalled = true;
        },
      },
      configurable: true,
    });
  }

  if (originalGBrowser && typeof originalGBrowser === "object") {
    // deno-lint-ignore no-explicit-any
    (originalGBrowser as any).addTab = (_url: string, _options: unknown) => {
      addTabCalled = true;
      return {};
    };
  } else {
    Object.defineProperty(globalThis, "gBrowser", {
      value: {
        addTab: (_url: string, _options: unknown) => {
          addTabCalled = true;
          return {};
        },
      },
      configurable: true,
    });
  }

  try {
    // deno-lint-ignore no-explicit-any
    (HubPanelMenu as any).handleOpenHub();

    assert(hideCalled, "handleOpenHub should call PanelUI.hide");
    assert(addTabCalled, "handleOpenHub should call gBrowser.addTab");
  } finally {
    cleanupDOM();
    if (originalPanelUI !== undefined) {
      Object.defineProperty(globalThis, "PanelUI", {
        value: originalPanelUI,
        configurable: true,
      });
    } else {
      // deno-lint-ignore no-explicit-any
      delete (globalThis as any).PanelUI;
    }
    if (originalGBrowser !== undefined) {
      Object.defineProperty(globalThis, "gBrowser", {
        value: originalGBrowser,
        configurable: true,
      });
    } else {
      // deno-lint-ignore no-explicit-any
      delete (globalThis as any).gBrowser;
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("HubPanelMenu.test.ts", [
    { name: "HubPanelMenu exports", fn: testHubPanelMenuExports },
    {
      name: "constructor does not throw without UI elements",
      fn: testHubPanelMenuDoesNotThrowWithoutPanelUIButton,
    },
    {
      name: "static render creates toolbar button",
      fn: testStaticRenderCreatesToolbarButton,
    },
    {
      name: "constructor does not throw with panel UI elements",
      fn: testHubPanelMenuConstructsWithPanelButton,
    },
    {
      name: "handleOpenHub opens hub and hides panel",
      fn: testHandleOpenHubAddsTabAndHidesPanel,
    },
  ]);
}
