// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { zFloorpDesignConfigs } from "../type.ts";
import { isRight, isLeft } from "fp-ts/Either";
import {
  assertEquals,
  assert,
  runTests,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** A fully valid config object that satisfies zFloorpDesignConfigs */
function makeValidConfig(): Record<string, unknown> {
  return {
    globalConfigs: {
      faviconColor: false,
      userInterface: "lepton",
      appliedUserJs: "",
    },
    tabbar: {
      tabbarStyle: "horizontal",
      tabbarPosition: "default",
      multiRowTabBar: { maxRowEnabled: false, maxRow: 3 },
    },
    tab: {
      tabScroll: { enabled: false, reverse: false, wrap: false },
      tabMinHeight: 30,
      tabMinWidth: 76,
      tabPinTitle: false,
      tabDubleClickToClose: false,
      tabOpenPosition: -1,
    },
    uiCustomization: {
      navbar: { position: "top", searchBarTop: false },
      display: {
        disableFullscreenNotification: false,
        deleteBrowserBorder: false,
      },
      special: {
        optimizeForTreeStyleTab: false,
        hideForwardBackwardButton: false,
        stgLikeWorkspaces: false,
      },
      multirowTab: { newtabInsideEnabled: false },
      bookmarkBar: { focusExpand: false },
      qrCode: { disableButton: false },
    },
  };
}

// ---------------------------------------------------------------------------
// Tests — valid configs
// ---------------------------------------------------------------------------

function testValidConfigDecodes(): void {
  const result = zFloorpDesignConfigs.decode(makeValidConfig());
  assert(isRight(result), "valid config should decode successfully");
}

function testAllUserInterfaceValues(): void {
  const uiValues = ["fluerial", "lepton", "photon", "protonfix", "proton"];
  for (const ui of uiValues) {
    const config = makeValidConfig();
    (config.globalConfigs as Record<string, unknown>).userInterface = ui;
    const result = zFloorpDesignConfigs.decode(config);
    assert(isRight(result), `userInterface="${ui}" should be valid`);
  }
}

function testAllTabbarStyleValues(): void {
  const styles = ["horizontal", "vertical", "multirow"];
  for (const style of styles) {
    const config = makeValidConfig();
    (config.tabbar as Record<string, unknown>).tabbarStyle = style;
    const result = zFloorpDesignConfigs.decode(config);
    assert(isRight(result), `tabbarStyle="${style}" should be valid`);
  }
}

function testAllTabbarPositionValues(): void {
  const positions = [
    "hide-horizontal-tabbar",
    "optimise-to-vertical-tabbar",
    "bottom-of-navigation-toolbar",
    "bottom-of-window",
    "default",
  ];
  for (const pos of positions) {
    const config = makeValidConfig();
    (config.tabbar as Record<string, unknown>).tabbarPosition = pos;
    const result = zFloorpDesignConfigs.decode(config);
    assert(isRight(result), `tabbarPosition="${pos}" should be valid`);
  }
}

function testAllNavbarPositions(): void {
  for (const pos of ["top", "bottom"] as const) {
    const config = makeValidConfig();
    const uic = config.uiCustomization as Record<string, unknown>;
    (uic.navbar as Record<string, unknown>).position = pos;
    const result = zFloorpDesignConfigs.decode(config);
    assert(isRight(result), `navbar position="${pos}" should be valid`);
  }
}

function testBookmarkBarPositionOptional(): void {
  // bookmarkBar.position is optional — config without it should still be valid
  const config = makeValidConfig();
  const uic = config.uiCustomization as Record<string, unknown>;
  (uic.bookmarkBar as Record<string, unknown>).focusExpand = true;
  // position is not set at all
  const result = zFloorpDesignConfigs.decode(config);
  assert(
    isRight(result),
    "config without bookmarkBar.position should be valid",
  );
}

// ---------------------------------------------------------------------------
// Tests — unknown fields preserved
// ---------------------------------------------------------------------------

function testUnknownFieldsPreserved(): void {
  const config = makeValidConfig();
  config.extraTopLevel = "should survive";
  (config.globalConfigs as Record<string, unknown>).extraGlobal = 42;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isRight(result), "config with extra fields should be valid");
  const decoded = result.right;
  assertEquals(
    (decoded as Record<string, unknown>).extraTopLevel as string,
    "should survive",
    "extra top-level field should be preserved",
  );
  assertEquals(
    (
      (decoded as Record<string, unknown>).globalConfigs as Record<
        string,
        unknown
      >
    ).extraGlobal as number,
    42,
    "extra globalConfigs field should be preserved",
  );
}

// ---------------------------------------------------------------------------
// Tests — invalid configs rejected
// ---------------------------------------------------------------------------

function testInvalidUserInterface(): void {
  const config = makeValidConfig();
  (config.globalConfigs as Record<string, unknown>).userInterface = "unknown";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "invalid userInterface should be rejected");
}

function testInvalidTabbarStyle(): void {
  const config = makeValidConfig();
  (config.tabbar as Record<string, unknown>).tabbarStyle = "circular";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "invalid tabbarStyle should be rejected");
}

function testInvalidTabbarPosition(): void {
  const config = makeValidConfig();
  (config.tabbar as Record<string, unknown>).tabbarPosition = "left";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "invalid tabbarPosition should be rejected");
}

function testInvalidNavbarPosition(): void {
  const config = makeValidConfig();
  const uic = config.uiCustomization as Record<string, unknown>;
  (uic.navbar as Record<string, unknown>).position = "middle";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "invalid navbar position should be rejected");
}

function testMissingGlobalConfigs(): void {
  const config = makeValidConfig();
  delete config.globalConfigs;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "missing globalConfigs should be rejected");
}

function testMissingTabbar(): void {
  const config = makeValidConfig();
  delete config.tabbar;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "missing tabbar should be rejected");
}

function testMissingTab(): void {
  const config = makeValidConfig();
  delete config.tab;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "missing tab should be rejected");
}

function testMissingUiCustomization(): void {
  const config = makeValidConfig();
  delete config.uiCustomization;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "missing uiCustomization should be rejected");
}

function testWrongTypeFaviconColor(): void {
  const config = makeValidConfig();
  (config.globalConfigs as Record<string, unknown>).faviconColor = "yes";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "string faviconColor should be rejected");
}

function testWrongTypeAppliedUserJs(): void {
  const config = makeValidConfig();
  (config.globalConfigs as Record<string, unknown>).appliedUserJs = 123;
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "number appliedUserJs should be rejected");
}

function testWrongTypeTabMinHeight(): void {
  const config = makeValidConfig();
  (config.tab as Record<string, unknown>).tabMinHeight = "30px";
  const result = zFloorpDesignConfigs.decode(config);
  assert(isLeft(result), "string tabMinHeight should be rejected");
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  await runTests("type.test.ts", [
    // valid configs
    { name: "valid config decodes", fn: testValidConfigDecodes },
    { name: "all userInterface values", fn: testAllUserInterfaceValues },
    { name: "all tabbarStyle values", fn: testAllTabbarStyleValues },
    { name: "all tabbarPosition values", fn: testAllTabbarPositionValues },
    { name: "all navbar positions", fn: testAllNavbarPositions },
    {
      name: "bookmarkBar position optional",
      fn: testBookmarkBarPositionOptional,
    },
    // unknown fields
    { name: "unknown fields preserved", fn: testUnknownFieldsPreserved },
    // invalid configs
    { name: "invalid userInterface rejected", fn: testInvalidUserInterface },
    { name: "invalid tabbarStyle rejected", fn: testInvalidTabbarStyle },
    { name: "invalid tabbarPosition rejected", fn: testInvalidTabbarPosition },
    { name: "invalid navbar position rejected", fn: testInvalidNavbarPosition },
    { name: "missing globalConfigs rejected", fn: testMissingGlobalConfigs },
    { name: "missing tabbar rejected", fn: testMissingTabbar },
    { name: "missing tab rejected", fn: testMissingTab },
    {
      name: "missing uiCustomization rejected",
      fn: testMissingUiCustomization,
    },
    { name: "wrong type faviconColor rejected", fn: testWrongTypeFaviconColor },
    {
      name: "wrong type appliedUserJs rejected",
      fn: testWrongTypeAppliedUserJs,
    },
    { name: "wrong type tabMinHeight rejected", fn: testWrongTypeTabMinHeight },
  ]);
}
