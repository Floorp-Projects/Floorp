// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assert,
  assertNotEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  patternToString,
  stringToPattern,
  defaultConfig,
  strDefaultConfig,
  MOUSE_GESTURE_ENABLED_PREF,
  MOUSE_GESTURE_CONFIG_PREF,
  MouseGestureConfigCodec,
  GestureDirectionCodec,
  GestureActionCodec,
} from "../config.ts";
import type {
  GestureDirection,
  GesturePattern,
  MouseGestureConfig,
} from "../config.ts";

// ---------------------------------------------------------------------------
// patternToString / stringToPattern tests
// ---------------------------------------------------------------------------

function testPatternToStringSingle(): void {
  assertEquals(patternToString(["left"]), "left", "single direction");
}

function testPatternToStringMultiple(): void {
  assertEquals(
    patternToString(["up", "right"]),
    "up-right",
    "two directions joined with dash",
  );
}

function testPatternToStringThree(): void {
  assertEquals(
    patternToString(["down", "right", "up"]),
    "down-right-up",
    "three directions",
  );
}

function testPatternToStringEmpty(): void {
  assertEquals(
    patternToString([]),
    "",
    "empty pattern should return empty string",
  );
}

function testPatternToStringDiagonal(): void {
  assertEquals(
    patternToString(["upRight", "downLeft"]),
    "upRight-downLeft",
    "diagonal directions",
  );
}

function testPatternToStringLongPattern(): void {
  assertEquals(
    patternToString(["right", "down", "left", "up", "right"]),
    "right-down-left-up-right",
    "five-direction pattern",
  );
}

function testStringToPatternSingle(): void {
  const result = stringToPattern("left");
  assertEquals(result.length, 1, "single direction length");
  assertEquals(result[0], "left", "single direction value");
}

function testStringToPatternMultiple(): void {
  const result = stringToPattern("up-right");
  assertEquals(result.length, 2, "two directions length");
  assertEquals(result[0], "up", "first direction");
  assertEquals(result[1], "right", "second direction");
}

function testStringToPatternEmpty(): void {
  const result = stringToPattern("");
  assertEquals(result.length, 1, "empty string splits to ['']");
}

function testRoundTripPattern(): void {
  const original: GesturePattern = ["left", "down", "right"];
  const str = patternToString(original);
  const restored = stringToPattern(str);
  assertEquals(restored.length, original.length, "round-trip length preserved");
  for (let i = 0; i < original.length; i++) {
    assertEquals(restored[i], original[i], `round-trip element ${i}`);
  }
}

function testRoundTripSingleDirection(): void {
  const original: GesturePattern = ["up"];
  assertEquals(
    stringToPattern(patternToString(original))[0],
    "up",
    "single round-trip",
  );
}

function testRoundTripDiagonalPattern(): void {
  const original: GesturePattern = ["upRight", "downLeft", "left"];
  const restored = stringToPattern(patternToString(original));
  assertEquals(restored.length, original.length, "diagonal round-trip length");
  assertEquals(restored[0], "upRight", "diagonal round-trip first");
  assertEquals(restored[1], "downLeft", "diagonal round-trip second");
  assertEquals(restored[2], "left", "diagonal round-trip third");
}

// ---------------------------------------------------------------------------
// GestureDirectionCodec tests
// ---------------------------------------------------------------------------

function testGestureDirectionCodecValidDirections(): void {
  const validDirections: GestureDirection[] = [
    "up",
    "down",
    "left",
    "right",
    "upRight",
    "upLeft",
    "downRight",
    "downLeft",
  ];
  for (const dir of validDirections) {
    const result = GestureDirectionCodec.decode(dir);
    assert(
      result._tag === "Right",
      `${dir} should be a valid GestureDirection`,
    );
  }
}

function testGestureDirectionCodecInvalidDirection(): void {
  const result = GestureDirectionCodec.decode("invalid-direction");
  assert(result._tag === "Left", "invalid direction should fail validation");
}

function testGestureDirectionCodecRejectsNumber(): void {
  const result = GestureDirectionCodec.decode(42);
  assert(
    result._tag === "Left",
    "number should fail GestureDirection validation",
  );
}

function testGestureDirectionCodecRejectsNull(): void {
  const result = GestureDirectionCodec.decode(null);
  assert(
    result._tag === "Left",
    "null should fail GestureDirection validation",
  );
}

function testGestureDirectionCodecRejectsUppercase(): void {
  const result = GestureDirectionCodec.decode("UP");
  assert(result._tag === "Left", "UP (uppercase) should fail validation");
}

function testGestureDirectionCodecRejectsCamelCaseVariant(): void {
  const result = GestureDirectionCodec.decode("up-right");
  assert(
    result._tag === "Left",
    "up-right (hyphenated) should fail validation",
  );
}

// ---------------------------------------------------------------------------
// GestureActionCodec tests
// ---------------------------------------------------------------------------

function testGestureActionCodecValid(): void {
  const result = GestureActionCodec.decode({
    pattern: ["left"],
    action: "go-back",
  });
  assert(result._tag === "Right", "valid action should pass validation");
}

function testGestureActionCodecValidDiagonalPattern(): void {
  const result = GestureActionCodec.decode({
    pattern: ["upRight", "downLeft"],
    action: "custom",
  });
  assert(result._tag === "Right", "diagonal pattern should pass validation");
}

function testGestureActionCodecInvalidPatternType(): void {
  const result = GestureActionCodec.decode({
    pattern: "left", // Should be array
    action: "go-back",
  });
  assert(result._tag === "Left", "string pattern should fail validation");
}

function testGestureActionCodecInvalidActionType(): void {
  const result = GestureActionCodec.decode({
    pattern: ["left"],
    action: 123, // Should be string
  });
  assert(result._tag === "Left", "numeric action should fail validation");
}

function testGestureActionCodecMissingPattern(): void {
  const result = GestureActionCodec.decode({
    action: "go-back",
  });
  assert(result._tag === "Left", "missing pattern should fail validation");
}

function testGestureActionCodecMissingAction(): void {
  const result = GestureActionCodec.decode({
    pattern: ["left"],
  });
  assert(result._tag === "Left", "missing action should fail validation");
}

function testGestureActionCodecEmptyPattern(): void {
  const result = GestureActionCodec.decode({
    pattern: [],
    action: "go-back",
  });
  assert(
    result._tag === "Right",
    "empty pattern array should pass validation (no min length)",
  );
}

function testGestureActionCodecMultipleDirections(): void {
  const result = GestureActionCodec.decode({
    pattern: ["up", "right", "down", "left"],
    action: "complex-gesture",
  });
  assert(result._tag === "Right", "4-direction pattern should pass validation");
}

// ---------------------------------------------------------------------------
// defaultConfig structure tests
// ---------------------------------------------------------------------------

function testDefaultConfigStructure(): void {
  const config = defaultConfig;
  assert(
    typeof config.rockerGesturesEnabled === "boolean",
    "rockerGesturesEnabled should be boolean",
  );
  assert(
    typeof config.wheelGesturesEnabled === "boolean",
    "wheelGesturesEnabled should be boolean",
  );
  assert(
    typeof config.sensitivity === "number",
    "sensitivity should be number",
  );
  assert(typeof config.showTrail === "boolean", "showTrail should be boolean");
  assert(typeof config.showLabel === "boolean", "showLabel should be boolean");
  assert(typeof config.trailColor === "string", "trailColor should be string");
  assert(typeof config.trailWidth === "number", "trailWidth should be number");
  assert(Array.isArray(config.actions), "actions should be an array");
}

function testDefaultConfigSensitivityRange(): void {
  const s = defaultConfig.sensitivity;
  assert(s >= 1 && s <= 100, `sensitivity should be 1-100, got ${s}`);
}

function testDefaultConfigContextMenu(): void {
  const cm = defaultConfig.contextMenu;
  assert(typeof cm.minDistance === "number", "minDistance should be number");
  assert(
    typeof cm.preventionTimeout === "number",
    "preventionTimeout should be number",
  );
  assert(
    cm.minDistance >= 5,
    `minDistance should be >= 5, got ${cm.minDistance}`,
  );
}

function testDefaultConfigActionsNonEmpty(): void {
  assert(
    defaultConfig.actions.length > 0,
    "default actions should not be empty",
  );
}

function testDefaultConfigActionsStructure(): void {
  for (const action of defaultConfig.actions) {
    assert(Array.isArray(action.pattern), "action.pattern should be array");
    assert(action.pattern.length > 0, "action.pattern should not be empty");
    assert(typeof action.action === "string", "action.action should be string");
    assert(action.action.length > 0, "action.action should not be empty");
  }
}

function testDefaultConfigKnownActions(): void {
  const actionNames = defaultConfig.actions.map((a) => a.action);
  assert(actionNames.includes("gecko-back"), "should include gecko-back");
  assert(actionNames.includes("gecko-forward"), "should include gecko-forward");
  assert(actionNames.includes("gecko-reload"), "should include gecko-reload");
  assert(
    actionNames.includes("gecko-close-tab"),
    "should include gecko-close-tab",
  );
  assert(
    actionNames.includes("gecko-open-new-tab"),
    "should include gecko-open-new-tab",
  );
}

function testDefaultConfigActionPatterns(): void {
  const config = defaultConfig;
  // Find specific patterns
  const backAction = config.actions.find((a) => a.action === "gecko-back");
  assert(backAction !== undefined, "gecko-back should exist");
  assertEquals(
    backAction.pattern[0],
    "left",
    "gecko-back pattern should be [left]",
  );

  const forwardAction = config.actions.find(
    (a) => a.action === "gecko-forward",
  );
  assert(forwardAction !== undefined, "gecko-forward should exist");
  assertEquals(
    forwardAction.pattern[0],
    "right",
    "gecko-forward pattern should be [right]",
  );

  const reloadAction = config.actions.find((a) => a.action === "gecko-reload");
  assert(reloadAction !== undefined, "gecko-reload should exist");
  assertEquals(
    reloadAction.pattern.length,
    2,
    "gecko-reload should have 2-direction pattern",
  );
  assertEquals(reloadAction.pattern[0], "up", "gecko-reload first direction");
  assertEquals(
    reloadAction.pattern[1],
    "down",
    "gecko-reload second direction",
  );
}

function testDefaultConfigTrailColor(): void {
  const color = defaultConfig.trailColor;
  assert(color.startsWith("#"), "trailColor should start with #");
  assert(
    color.length === 7,
    `trailColor should be 7 chars (#RRGGBB), got ${color.length}`,
  );
}

function testDefaultConfigTrailWidth(): void {
  const w = defaultConfig.trailWidth;
  assert(w > 0, `trailWidth should be positive, got ${w}`);
  assert(Number.isInteger(w), "trailWidth should be integer");
}

function testDefaultConfigNineActions(): void {
  assertEquals(
    defaultConfig.actions.length,
    9,
    "default config should have exactly 9 actions",
  );
}

function testDefaultConfigActionPatternsUnique(): void {
  const patternStrings = defaultConfig.actions.map((a) =>
    patternToString(a.pattern),
  );
  const uniquePatterns = new Set(patternStrings);
  assertEquals(
    uniquePatterns.size,
    patternStrings.length,
    "all action patterns should be unique",
  );
}

// ---------------------------------------------------------------------------
// defaultConfig implied BASE_DEFAULT_CONFIG tests
// ---------------------------------------------------------------------------

function testDefaultConfigEnabledFalse(): void {
  // BASE_DEFAULT_CONFIG.enabled = false, normalizeConfig preserves it
  assertEquals(
    defaultConfig.enabled,
    false,
    "default config enabled should be false",
  );
}

function testDefaultConfigHasAllBaseFields(): void {
  const config = defaultConfig;
  assert(config.enabled !== undefined, "enabled should exist");
  assert(
    config.rockerGesturesEnabled !== undefined,
    "rockerGesturesEnabled should exist",
  );
  assert(
    config.wheelGesturesEnabled !== undefined,
    "wheelGesturesEnabled should exist",
  );
  assert(config.sensitivity !== undefined, "sensitivity should exist");
  assert(config.showTrail !== undefined, "showTrail should exist");
  assert(config.showLabel !== undefined, "showLabel should exist");
  assert(config.trailColor !== undefined, "trailColor should exist");
  assert(config.trailWidth !== undefined, "trailWidth should exist");
  assert(config.contextMenu !== undefined, "contextMenu should exist");
  assert(config.actions !== undefined, "actions should exist");
}

// ---------------------------------------------------------------------------
// MouseGestureConfigCodec validation tests
// ---------------------------------------------------------------------------

function testConfigCodecValidMinimal(): void {
  const validConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#ffffff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [{ pattern: ["left"], action: "back" }],
  };
  const result = MouseGestureConfigCodec.decode(validConfig);
  assert(result._tag === "Right", "valid config should pass validation");
}

function testConfigCodecValidWithOptionalEnabled(): void {
  const validConfig = {
    enabled: true,
    rockerGesturesEnabled: false,
    wheelGesturesEnabled: false,
    sensitivity: 80,
    showTrail: false,
    showLabel: false,
    trailColor: "#ff0000",
    trailWidth: 10,
    contextMenu: { minDistance: 10, preventionTimeout: 500 },
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(validConfig);
  assert(
    result._tag === "Right",
    "valid config with optional enabled should pass",
  );
}

function testConfigCodecValidEmptyActions(): void {
  const validConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(validConfig);
  assert(result._tag === "Right", "empty actions array should be valid");
}

function testConfigCodecInvalidMissingFields(): void {
  const invalidConfig = {
    sensitivity: 40,
    // Missing many required fields
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "missing required fields should fail validation",
  );
}

function testConfigCodecInvalidSensitivityType(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: "high", // Should be number
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "wrong sensitivity type should fail validation",
  );
}

function testConfigCodecInvalidActionPattern(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [{ pattern: ["invalid-direction"], action: "test" }],
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "invalid direction in pattern should fail validation",
  );
}

function testConfigCodecInvalidTrailWidthType(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: "thick", // Should be number
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "wrong trailWidth type should fail validation",
  );
}

function testConfigCodecInvalidShowTrailType(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: "yes", // Should be boolean
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(result._tag === "Left", "wrong showTrail type should fail validation");
}

function testConfigCodecInvalidContextMenuStructure(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: "five" }, // Should be number, missing preventionTimeout
    actions: [],
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "invalid contextMenu structure should fail validation",
  );
}

function testConfigCodecInvalidActionMissingAction(): void {
  const invalidConfig = {
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#fff",
    trailWidth: 3,
    contextMenu: { minDistance: 5, preventionTimeout: 200 },
    actions: [{ pattern: ["left"] }], // Missing "action" field
  };
  const result = MouseGestureConfigCodec.decode(invalidConfig);
  assert(
    result._tag === "Left",
    "action missing 'action' field should fail validation",
  );
}

// ---------------------------------------------------------------------------
// Pref constants tests
// ---------------------------------------------------------------------------

function testPrefConstants(): void {
  assertEquals(
    MOUSE_GESTURE_ENABLED_PREF,
    "floorp.mousegesture.enabled",
    "enabled pref",
  );
  assertEquals(
    MOUSE_GESTURE_CONFIG_PREF,
    "floorp.mousegesture.config",
    "config pref",
  );
}

function testStrDefaultConfigIsJson(): void {
  // Should be valid JSON
  const parsed = JSON.parse(strDefaultConfig);
  assert(typeof parsed === "object", "strDefaultConfig should parse to object");
  assert(parsed.actions !== undefined, "parsed config should have actions");
}

function testStrDefaultConfigRoundtrip(): void {
  const parsed = JSON.parse(strDefaultConfig);
  assertEquals(
    parsed.rockerGesturesEnabled,
    defaultConfig.rockerGesturesEnabled,
    "roundtrip rockerGesturesEnabled",
  );
  assertEquals(
    parsed.wheelGesturesEnabled,
    defaultConfig.wheelGesturesEnabled,
    "roundtrip wheelGesturesEnabled",
  );
  assertEquals(
    parsed.sensitivity,
    defaultConfig.sensitivity,
    "roundtrip sensitivity",
  );
  assertEquals(
    parsed.showTrail,
    defaultConfig.showTrail,
    "roundtrip showTrail",
  );
  assertEquals(
    parsed.showLabel,
    defaultConfig.showLabel,
    "roundtrip showLabel",
  );
  assertEquals(
    parsed.trailColor,
    defaultConfig.trailColor,
    "roundtrip trailColor",
  );
  assertEquals(
    parsed.trailWidth,
    defaultConfig.trailWidth,
    "roundtrip trailWidth",
  );
  assertEquals(
    parsed.actions.length,
    defaultConfig.actions.length,
    "roundtrip actions count",
  );
}

function testStrDefaultConfigActionsMatch(): void {
  const parsed = JSON.parse(strDefaultConfig);
  for (let i = 0; i < defaultConfig.actions.length; i++) {
    const original = defaultConfig.actions[i];
    const serialized = parsed.actions[i];
    assertEquals(serialized.action, original.action, `action ${i} name match`);
    assertEquals(
      serialized.pattern.length,
      original.pattern.length,
      `action ${i} pattern length`,
    );
    for (let j = 0; j < original.pattern.length; j++) {
      assertEquals(
        serialized.pattern[j],
        original.pattern[j],
        `action ${i} pattern[${j}]`,
      );
    }
  }
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  // patternToString
  { name: "patternToString single direction", fn: testPatternToStringSingle },
  {
    name: "patternToString multiple directions",
    fn: testPatternToStringMultiple,
  },
  { name: "patternToString three directions", fn: testPatternToStringThree },
  { name: "patternToString empty pattern", fn: testPatternToStringEmpty },
  {
    name: "patternToString diagonal directions",
    fn: testPatternToStringDiagonal,
  },
  {
    name: "patternToString five-direction pattern",
    fn: testPatternToStringLongPattern,
  },
  // stringToPattern
  { name: "stringToPattern single direction", fn: testStringToPatternSingle },
  {
    name: "stringToPattern multiple directions",
    fn: testStringToPatternMultiple,
  },
  { name: "stringToPattern empty string", fn: testStringToPatternEmpty },
  // round-trip
  { name: "round-trip pattern conversion", fn: testRoundTripPattern },
  { name: "round-trip single direction", fn: testRoundTripSingleDirection },
  { name: "round-trip diagonal pattern", fn: testRoundTripDiagonalPattern },
  // GestureDirectionCodec
  {
    name: "GestureDirectionCodec valid directions",
    fn: testGestureDirectionCodecValidDirections,
  },
  {
    name: "GestureDirectionCodec invalid direction",
    fn: testGestureDirectionCodecInvalidDirection,
  },
  {
    name: "GestureDirectionCodec rejects number",
    fn: testGestureDirectionCodecRejectsNumber,
  },
  {
    name: "GestureDirectionCodec rejects null",
    fn: testGestureDirectionCodecRejectsNull,
  },
  {
    name: "GestureDirectionCodec rejects uppercase",
    fn: testGestureDirectionCodecRejectsUppercase,
  },
  {
    name: "GestureDirectionCodec rejects hyphenated",
    fn: testGestureDirectionCodecRejectsCamelCaseVariant,
  },
  // GestureActionCodec
  { name: "GestureActionCodec valid", fn: testGestureActionCodecValid },
  {
    name: "GestureActionCodec valid diagonal pattern",
    fn: testGestureActionCodecValidDiagonalPattern,
  },
  {
    name: "GestureActionCodec invalid pattern type",
    fn: testGestureActionCodecInvalidPatternType,
  },
  {
    name: "GestureActionCodec invalid action type",
    fn: testGestureActionCodecInvalidActionType,
  },
  {
    name: "GestureActionCodec missing pattern",
    fn: testGestureActionCodecMissingPattern,
  },
  {
    name: "GestureActionCodec missing action",
    fn: testGestureActionCodecMissingAction,
  },
  {
    name: "GestureActionCodec empty pattern",
    fn: testGestureActionCodecEmptyPattern,
  },
  {
    name: "GestureActionCodec multiple directions",
    fn: testGestureActionCodecMultipleDirections,
  },
  // defaultConfig structure
  { name: "defaultConfig structure", fn: testDefaultConfigStructure },
  {
    name: "defaultConfig sensitivity range",
    fn: testDefaultConfigSensitivityRange,
  },
  { name: "defaultConfig contextMenu", fn: testDefaultConfigContextMenu },
  {
    name: "defaultConfig actions non-empty",
    fn: testDefaultConfigActionsNonEmpty,
  },
  {
    name: "defaultConfig actions structure",
    fn: testDefaultConfigActionsStructure,
  },
  { name: "defaultConfig known actions", fn: testDefaultConfigKnownActions },
  {
    name: "defaultConfig action patterns",
    fn: testDefaultConfigActionPatterns,
  },
  { name: "defaultConfig trailColor format", fn: testDefaultConfigTrailColor },
  { name: "defaultConfig trailWidth valid", fn: testDefaultConfigTrailWidth },
  { name: "defaultConfig nine actions", fn: testDefaultConfigNineActions },
  {
    name: "defaultConfig action patterns unique",
    fn: testDefaultConfigActionPatternsUnique,
  },
  // defaultConfig implied BASE fields
  { name: "defaultConfig enabled false", fn: testDefaultConfigEnabledFalse },
  {
    name: "defaultConfig has all base fields",
    fn: testDefaultConfigHasAllBaseFields,
  },
  // MouseGestureConfigCodec
  { name: "ConfigCodec valid minimal", fn: testConfigCodecValidMinimal },
  {
    name: "ConfigCodec valid with optional enabled",
    fn: testConfigCodecValidWithOptionalEnabled,
  },
  {
    name: "ConfigCodec valid empty actions",
    fn: testConfigCodecValidEmptyActions,
  },
  {
    name: "ConfigCodec invalid missing fields",
    fn: testConfigCodecInvalidMissingFields,
  },
  {
    name: "ConfigCodec invalid sensitivity type",
    fn: testConfigCodecInvalidSensitivityType,
  },
  {
    name: "ConfigCodec invalid action pattern",
    fn: testConfigCodecInvalidActionPattern,
  },
  {
    name: "ConfigCodec invalid trailWidth type",
    fn: testConfigCodecInvalidTrailWidthType,
  },
  {
    name: "ConfigCodec invalid showTrail type",
    fn: testConfigCodecInvalidShowTrailType,
  },
  {
    name: "ConfigCodec invalid contextMenu structure",
    fn: testConfigCodecInvalidContextMenuStructure,
  },
  {
    name: "ConfigCodec invalid action missing action field",
    fn: testConfigCodecInvalidActionMissingAction,
  },
  // pref constants
  { name: "pref constants", fn: testPrefConstants },
  { name: "strDefaultConfig is valid JSON", fn: testStrDefaultConfigIsJson },
  {
    name: "strDefaultConfig roundtrip values",
    fn: testStrDefaultConfigRoundtrip,
  },
  {
    name: "strDefaultConfig actions match",
    fn: testStrDefaultConfigActionsMatch,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureConfig.test.ts", tests);
}
