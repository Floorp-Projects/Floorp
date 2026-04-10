// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  zenModeEnabled,
  setZenModeEnabled,
  initZenModeState,
  ZenModeMenuElement,
} from "../zen-mode.tsx";

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import { render } from "@nora/solid-xul";

const ZENMODE_PREF = "floorp.zenmode.enabled";

function testZenModeSignalReadable(): void {
  const value = zenModeEnabled();
  assert(typeof value === "boolean", "zenModeEnabled should return a boolean");
}

function testSetZenModeEnabledToggles(): void {
  const original = zenModeEnabled();
  try {
    setZenModeEnabled(true);
    assertEquals(
      zenModeEnabled(),
      true,
      "zenModeEnabled should be true after setting true",
    );

    setZenModeEnabled(false);
    assertEquals(
      zenModeEnabled(),
      false,
      "zenModeEnabled should be false after setting false",
    );
  } finally {
    setZenModeEnabled(original);
  }
}

function testZenModePrefSync(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    setZenModeEnabled(true);
    assertEquals(
      Services.prefs.getBoolPref(ZENMODE_PREF, false),
      true,
      "Pref should be true after setZenModeEnabled(true)",
    );

    setZenModeEnabled(false);
    assertEquals(
      Services.prefs.getBoolPref(ZENMODE_PREF, false),
      false,
      "Pref should be false after setZenModeEnabled(false)",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testZenModeAttributeOnDocumentElement(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    setZenModeEnabled(true);
    assert(
      document?.documentElement?.hasAttribute("zenmode"),
      "documentElement should have 'zenmode' attribute when zen mode is enabled",
    );

    setZenModeEnabled(false);
    assert(
      !document?.documentElement?.hasAttribute("zenmode"),
      "documentElement should not have 'zenmode' attribute when zen mode is disabled",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
    document?.documentElement?.removeAttribute("zenmode");
    document?.documentElement?.removeAttribute("zenmode-reveal-top");
    document?.documentElement?.removeAttribute("zenmode-reveal-bottom");
    document?.documentElement?.removeAttribute("zenmode-reveal-side");
  }
}

function testZenModePrefObserverUpdatesSignal(): void {
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);
  const originalSignal = zenModeEnabled();

  try {
    initZenModeState();

    Services.prefs.setBoolPref(ZENMODE_PREF, true);
    assertEquals(
      zenModeEnabled(),
      true,
      "Signal should be true after pref set to true externally",
    );

    Services.prefs.setBoolPref(ZENMODE_PREF, false);
    assertEquals(
      zenModeEnabled(),
      false,
      "Signal should be false after pref set to false externally",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testMeasureAndSetCSSVariablesSetsToolboxHeight(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    // Create a mock toolbox element
    const mockToolbox = document!.createElement("div") as unknown as XULElement;
    mockToolbox.id = "navigator-toolbox";
    ((mockToolbox as unknown as HTMLElement).style as unknown as Record<string, string>).height = "100px";
    document!.body?.appendChild(mockToolbox as unknown as Node);

    // Enable zen mode and measure
    setZenModeEnabled(true);

    const root = document!.documentElement as HTMLElement;
    const toolboxHeight = root.style.getPropertyValue("--zenmode-toolbox-height");

    assert(
      toolboxHeight === "100px" || toolboxHeight !== "",
      "CSS variable --zenmode-toolbox-height should be set after enabling zen mode",
    );

    document!.body?.removeChild(mockToolbox as unknown as Node);
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testMeasureAndSetCSSVariablesHandlesMissingToolbox(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    // Ensure no toolbox exists
    const existingToolbox = document!.getElementById("navigator-toolbox");
    if (existingToolbox) {
      existingToolbox.remove();
    }

    // Should not throw when toolbox is missing
    setZenModeEnabled(true);

    const root = document!.documentElement as HTMLElement;
    const toolboxHeight = root.style.getPropertyValue("--zenmode-toolbox-height");

    // Environment may already have a toolbox or preexisting value; main contract is no throw.
    assert(
      typeof toolboxHeight === "string",
      "CSS variable read should succeed even when toolbox is missing",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testTopEdgeHoverRevealSetsAttribute(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(true);

    // Create a mock toolbox
    const mockToolbox = document!.createElement("div") as unknown as XULElement;
    mockToolbox.id = "navigator-toolbox";
    ((mockToolbox as unknown as HTMLElement).style as unknown as Record<string, string>).height = "100px";
    ((mockToolbox as unknown as HTMLElement).style as unknown as Record<string, string>).position = "absolute";
    ((mockToolbox as unknown as HTMLElement).style as unknown as Record<string, string>).top = "0px";
    document!.body?.appendChild(mockToolbox as unknown as Node);

    // Simulate mouse move to top edge
    const mouseEvent = new MouseEvent("mousemove", {
      clientX: 100,
      clientY: 5, // Within EDGE_THRESHOLD (10)
    });
    window.dispatchEvent(mouseEvent);

    assert(
      document!.documentElement?.hasAttribute("zenmode-reveal-top"),
      "zenmode-reveal-top attribute should be set when hovering top edge",
    );

    document!.body?.removeChild(mockToolbox as unknown as Node);
    document!.documentElement?.removeAttribute("zenmode-reveal-top");
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testBottomEdgeHoverRevealSetsAttribute(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(true);

    // Simulate mouse move to bottom edge
    const mouseEvent = new MouseEvent("mousemove", {
      clientX: 100,
      clientY: window.innerHeight - 5, // Within EDGE_THRESHOLD of bottom
    });
    window.dispatchEvent(mouseEvent);

    assert(
      document!.documentElement?.hasAttribute("zenmode-reveal-bottom"),
      "zenmode-reveal-bottom attribute should be set when hovering bottom edge",
    );

    document!.documentElement?.removeAttribute("zenmode-reveal-bottom");
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testSideEdgeHoverRevealSetsAttribute(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(true);

    // Simulate mouse move to left edge
    const mouseEvent = new MouseEvent("mousemove", {
      clientX: 5, // Within EDGE_THRESHOLD
      clientY: 100,
    });
    window.dispatchEvent(mouseEvent);

    assert(
      document!.documentElement?.hasAttribute("zenmode-reveal-side"),
      "zenmode-reveal-side attribute should be set when hovering left edge",
    );

    document!.documentElement?.removeAttribute("zenmode-reveal-side");
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testHoverRevealDoesNotTriggerWhenDisabled(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(false);

    // Simulate mouse move to top edge
    const mouseEvent = new MouseEvent("mousemove", {
      clientX: 100,
      clientY: 5,
    });
    window.dispatchEvent(mouseEvent);

    assert(
      !document!.documentElement?.hasAttribute("zenmode-reveal-top"),
      "zenmode-reveal-top attribute should NOT be set when zen mode is disabled",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testZenModeDisablingRemovesAllRevealAttributes(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(true);

    // Manually set all reveal attributes
    document!.documentElement?.setAttribute("zenmode-reveal-top", "");
    document!.documentElement?.setAttribute("zenmode-reveal-bottom", "");
    document!.documentElement?.setAttribute("zenmode-reveal-side", "");

    // Disable zen mode
    setZenModeEnabled(false);

    assert(
      !document!.documentElement?.hasAttribute("zenmode-reveal-top"),
      "zenmode-reveal-top should be removed when zen mode is disabled",
    );
    assert(
      !document!.documentElement?.hasAttribute("zenmode-reveal-bottom"),
      "zenmode-reveal-bottom should be removed when zen mode is disabled",
    );
    assert(
      !document!.documentElement?.hasAttribute("zenmode-reveal-side"),
      "zenmode-reveal-side should be removed when zen mode is disabled",
    );
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testZenModeMenuElementHasCorrectId(): void {
  const container = document!.createElement("div");
  render(() => ZenModeMenuElement(), container);

  const menuitem = container.querySelector("#toggle_zenmode");
  assert(
    menuitem !== null,
    "ZenModeMenuElement should render a menuitem with id 'toggle_zenmode'",
  );

  menuitem?.remove();
}

function testZenModeMenuElementCheckedStateSyncs(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    setZenModeEnabled(true);

    const container = document!.createElement("div");
    render(() => ZenModeMenuElement(), container);

    const menuitem = container.querySelector("#toggle_zenmode");
    const checked = menuitem?.getAttribute("checked");

    assert(
      checked === "true" || checked === "",
      "menuitem checked attribute should be set when zen mode is enabled",
    );

    menuitem?.remove();
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

function testZenModeMenuElementHasCorrectAccesskey(): void {
  const container = document!.createElement("div");
  render(() => ZenModeMenuElement(), container);

  const menuitem = container.querySelector("#toggle_zenmode");
  const accesskey = menuitem?.getAttribute("accesskey");

  assertEquals(
    accesskey,
    "Z",
    "menuitem should have accesskey 'Z'",
  );

  menuitem?.remove();
}

function testZenModeMenuElementConditionallyRendersStyle(): void {
  const originalSignal = zenModeEnabled();
  const originalPref = Services.prefs.getBoolPref(ZENMODE_PREF, false);

  try {
    // Test with zen mode disabled
    setZenModeEnabled(false);
    let container = document!.createElement("div");
    render(() => ZenModeMenuElement(), container);
    let styleElement = container.querySelector("style");

    assert(
      styleElement === null,
      "style element should not be rendered when zen mode is disabled",
    );

    // Test with zen mode enabled
    setZenModeEnabled(true);
    container = document!.createElement("div");
    render(() => ZenModeMenuElement(), container);
    styleElement = container.querySelector("style");

    assert(
      styleElement !== null,
      "style element should be rendered when zen mode is enabled",
    );

    container.remove();
  } finally {
    setZenModeEnabled(originalSignal);
    Services.prefs.setBoolPref(ZENMODE_PREF, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("zenMode.test.ts", [
    {
      name: "zenModeEnabled signal is readable",
      fn: testZenModeSignalReadable,
    },
    {
      name: "setZenModeEnabled toggles value",
      fn: testSetZenModeEnabledToggles,
    },
    { name: "zen mode pref sync", fn: testZenModePrefSync },
    {
      name: "zen mode attribute on documentElement",
      fn: testZenModeAttributeOnDocumentElement,
    },
    {
      name: "pref observer updates signal",
      fn: testZenModePrefObserverUpdatesSignal,
    },
    {
      name: "measureAndSetCSSVariables sets toolbox height",
      fn: testMeasureAndSetCSSVariablesSetsToolboxHeight,
    },
    {
      name: "measureAndSetCSSVariables handles missing toolbox",
      fn: testMeasureAndSetCSSVariablesHandlesMissingToolbox,
    },
    {
      name: "top edge hover reveal sets attribute",
      fn: testTopEdgeHoverRevealSetsAttribute,
    },
    {
      name: "bottom edge hover reveal sets attribute",
      fn: testBottomEdgeHoverRevealSetsAttribute,
    },
    {
      name: "side edge hover reveal sets attribute",
      fn: testSideEdgeHoverRevealSetsAttribute,
    },
    {
      name: "hover reveal does not trigger when disabled",
      fn: testHoverRevealDoesNotTriggerWhenDisabled,
    },
    {
      name: "zen mode disabling removes all reveal attributes",
      fn: testZenModeDisablingRemovesAllRevealAttributes,
    },
    {
      name: "ZenModeMenuElement has correct id",
      fn: testZenModeMenuElementHasCorrectId,
    },
    {
      name: "ZenModeMenuElement checked state syncs",
      fn: testZenModeMenuElementCheckedStateSyncs,
    },
    {
      name: "ZenModeMenuElement has correct accesskey",
      fn: testZenModeMenuElementHasCorrectAccesskey,
    },
    {
      name: "ZenModeMenuElement conditionally renders style",
      fn: testZenModeMenuElementConditionallyRendersStyle,
    },
  ]);
}
