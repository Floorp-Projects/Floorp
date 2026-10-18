// SPDX-License-Identifier: MPL-2.0

/**
 * Host-safe tests for NRKeySyncCombo (pure conversions, no Firefox APIs).
 * The colocated runner executes this file inside the esm layer, but the
 * tested utilities never touch browser-only globals, so the suite also runs
 * directly on Deno.
 */

import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import {
  codeToXulKey,
  comboToCanonicalCode,
  formatXulComboText,
  modifiersToXulString,
  type Modifiers,
  type XulCombo,
  type XulKeyDefinition,
  xulComboToCanonicalCode,
  xulKeyToCode,
  xulStringToModifiers,
} from "./NRKeySyncCombo.ts";

function makeModifiers(
  alt: boolean,
  ctrl: boolean,
  meta: boolean,
  shift: boolean,
): Modifiers {
  return { alt, ctrl, meta, shift };
}

function assertSameModifiers(
  actual: Modifiers,
  expected: Modifiers,
  message: string,
): void {
  assertEquals(actual.alt, expected.alt, `${message}: alt`);
  assertEquals(actual.ctrl, expected.ctrl, `${message}: ctrl`);
  assertEquals(actual.meta, expected.meta, `${message}: meta`);
  assertEquals(actual.shift, expected.shift, `${message}: shift`);
}

function xulKeySignature(definition: XulKeyDefinition | null): string {
  if (definition === null) {
    return "null";
  }
  if ("key" in definition) {
    return `key:${definition.key}`;
  }
  return `keycode:${definition.keycode}`;
}

// [event.code, expected codeToXulKey signature]
const CODE_ROUND_TRIP_CASES: Array<[string, string]> = [
  ["KeyA", "key:A"],
  ["KeyZ", "key:Z"],
  ["Digit0", "key:0"],
  ["Digit9", "key:9"],
  ["F1", "keycode:VK_F1"],
  ["F5", "keycode:VK_F5"],
  ["F24", "keycode:VK_F24"],
  ["ArrowUp", "keycode:VK_UP"],
  ["ArrowDown", "keycode:VK_DOWN"],
  ["ArrowLeft", "keycode:VK_LEFT"],
  ["ArrowRight", "keycode:VK_RIGHT"],
  ["Backspace", "keycode:VK_BACK_SPACE"],
  ["Enter", "keycode:VK_RETURN"],
  ["Escape", "keycode:VK_ESCAPE"],
  ["Tab", "keycode:VK_TAB"],
  ["Space", "key: "],
  ["Delete", "keycode:VK_DELETE"],
  ["Insert", "keycode:VK_INSERT"],
  ["Home", "keycode:VK_HOME"],
  ["End", "keycode:VK_END"],
  ["PageUp", "keycode:VK_PAGE_UP"],
  ["PageDown", "keycode:VK_PAGE_DOWN"],
  ["Minus", "key:-"],
  ["Equal", "key:="],
  ["BracketLeft", "key:["],
  ["BracketRight", "key:]"],
  ["Semicolon", "key:;"],
  ["Quote", "key:'"],
  ["Backquote", "key:`"],
  ["Comma", "key:,"],
  ["Period", "key:."],
  ["Slash", "key:/"],
  ["Backslash", "key:\\"],
  ["Numpad0", "keycode:VK_NUMPAD0"],
  ["Numpad9", "keycode:VK_NUMPAD9"],
  ["NumpadAdd", "keycode:VK_ADD"],
  ["NumpadSubtract", "keycode:VK_SUBTRACT"],
  ["NumpadMultiply", "keycode:VK_MULTIPLY"],
  ["NumpadDivide", "keycode:VK_DIVIDE"],
  ["NumpadDecimal", "keycode:VK_DECIMAL"],
];

const MODIFIER_ROUND_TRIP_CASES: Modifiers[] = [
  makeModifiers(false, false, false, false),
  makeModifiers(false, true, false, false),
  makeModifiers(true, false, false, false),
  makeModifiers(false, false, true, false),
  makeModifiers(false, false, false, true),
  makeModifiers(true, true, false, true),
  makeModifiers(false, true, true, false),
  makeModifiers(true, true, true, true),
];

function testModifiersToXulStringPerPlatform(): void {
  assertEquals(
    modifiersToXulString(makeModifiers(false, true, false, false), false),
    "accel",
    "ctrl maps to accel off macOS",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(false, true, false, false), true),
    "control",
    "ctrl maps to control on macOS",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(false, false, true, false), false),
    "meta",
    "meta maps to meta off macOS",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(false, false, true, false), true),
    "accel",
    "meta maps to accel on macOS",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(true, false, false, true), false),
    "alt,shift",
    "alt and shift stay literal",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(true, true, false, true), false),
    "accel,alt,shift",
    "tokens are sorted alphabetically",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(true, false, true, true), true),
    "accel,alt,shift",
    "mac tokens are sorted alphabetically",
  );
  assertEquals(
    modifiersToXulString(makeModifiers(false, false, false, false), false),
    "",
    "no modifiers yields an empty string",
  );
}

function testXulStringToModifiersParsesPlatformAware(): void {
  assertSameModifiers(
    xulStringToModifiers("accel", false),
    makeModifiers(false, true, false, false),
    "accel is ctrl off macOS",
  );
  assertSameModifiers(
    xulStringToModifiers("accel", true),
    makeModifiers(false, false, true, false),
    "accel is meta on macOS",
  );
  assertSameModifiers(
    xulStringToModifiers("control", true),
    makeModifiers(false, true, false, false),
    "control stays ctrl on macOS",
  );
  assertSameModifiers(
    xulStringToModifiers("meta", true),
    makeModifiers(false, false, true, false),
    "meta stays meta on macOS",
  );
  assertSameModifiers(
    xulStringToModifiers("os,alt", false),
    makeModifiers(true, false, false, false),
    "os token is ignored",
  );
  assertSameModifiers(
    xulStringToModifiers(" ACCEL , Shift ", false),
    makeModifiers(false, true, false, true),
    "tokens are trimmed and lowercased",
  );
  assertSameModifiers(
    xulStringToModifiers("hyper,shift", false),
    makeModifiers(false, false, false, true),
    "unknown tokens are ignored",
  );
  assertSameModifiers(
    xulStringToModifiers("", false),
    makeModifiers(false, false, false, false),
    "empty string yields no modifiers",
  );
}

function testModifierConversionRoundTripsBothPlatforms(): void {
  for (const isMac of [false, true]) {
    for (const combo of MODIFIER_ROUND_TRIP_CASES) {
      const xul = modifiersToXulString(combo, isMac);
      assertSameModifiers(
        xulStringToModifiers(xul, isMac),
        combo,
        `round trip (isMac=${isMac}) "${xul}"`,
      );
    }
  }
}

function testCodeToXulKeyForwardMapping(): void {
  for (const [code, signature] of CODE_ROUND_TRIP_CASES) {
    assertEquals(
      xulKeySignature(codeToXulKey(code)),
      signature,
      `codeToXulKey(${code})`,
    );
  }
}

function testXulKeyToCodeReverseMapping(): void {
  const reverseCases: Array<[XulKeyDefinition, string]> = [
    [{ key: "A" }, "KeyA"],
    [{ key: "1" }, "Digit1"],
    [{ key: " " }, "Space"],
    [{ key: "-" }, "Minus"],
    [{ key: "\\" }, "Backslash"],
    [{ keycode: "VK_F5" }, "F5"],
    [{ keycode: "VK_UP" }, "ArrowUp"],
    [{ keycode: "VK_BACK_SPACE" }, "Backspace"],
    [{ keycode: "VK_RETURN" }, "Enter"],
    [{ keycode: "VK_PAGE_UP" }, "PageUp"],
    [{ keycode: "VK_NUMPAD0" }, "Numpad0"],
    [{ keycode: "VK_DECIMAL" }, "NumpadDecimal"],
  ];

  for (const [definition, expectedCode] of reverseCases) {
    assertEquals(
      xulKeyToCode(definition),
      expectedCode,
      `xulKeyToCode(${xulKeySignature(definition)})`,
    );
  }

  for (const [code] of CODE_ROUND_TRIP_CASES) {
    const definition = codeToXulKey(code);
    if (definition === null) {
      throw new Error(`codeToXulKey(${code}) unexpectedly returned null`);
    }
    assertEquals(
      xulKeyToCode(definition),
      code,
      `round trip through XUL for ${code}`,
    );
  }
}

function testUnknownCodesAndKeysReturnNull(): void {
  assertEquals(codeToXulKey("MediaPlay"), null, "unknown code returns null");
  assertEquals(codeToXulKey("Key1"), null, "Key1 is not an event.code");
  assertEquals(codeToXulKey("F25"), null, "beyond F24 is unsupported");
  assertEquals(codeToXulKey(""), null, "empty code returns null");
  assertEquals(
    codeToXulKey("toString"),
    null,
    "inherited property names must not leak through the table",
  );
  assertEquals(xulKeyToCode({}), null, "missing key and keycode returns null");
  assertEquals(xulKeyToCode({ key: "" }), null, "empty key returns null");
  assertEquals(xulKeyToCode({ key: "QQ" }), null, "unknown key returns null");
  assertEquals(
    xulKeyToCode({ keycode: "VK_MEDIA_PLAY" }),
    null,
    "unknown keycode returns null",
  );
  assertEquals(
    xulKeyToCode({ keycode: "" }),
    null,
    "empty keycode returns null",
  );
  assertEquals(
    xulKeyToCode({ key: "toString" }),
    null,
    "inherited keys must not resolve",
  );
}

function testCanonicalCodesMatchAcrossFormats(): void {
  const floorp = comboToCanonicalCode(
    makeModifiers(false, true, false, true),
    "KeyT",
  );
  assertEquals(floorp, "ctrl+shift+KeyT", "floorp canonical form");
  assertEquals(
    xulComboToCanonicalCode({ modifiers: "accel,shift", key: "T" }, false),
    floorp,
    "XUL accel,shift+T equals Floorp ctrl+shift+KeyT off macOS",
  );
  assertEquals(
    comboToCanonicalCode(
      makeModifiers(true, false, false, false),
      "KeyZ",
    ),
    "alt+KeyZ",
    "canonical order uses fixed modifier sequence",
  );
  assertEquals(
    comboToCanonicalCode(
      makeModifiers(true, true, true, true),
      "KeyZ",
    ),
    "alt+ctrl+meta+shift+KeyZ",
    "all modifiers appear in fixed order",
  );
  assertEquals(
    comboToCanonicalCode(makeModifiers(false, false, false, false), "F2"),
    "F2",
    "combo without modifiers is the bare code",
  );
  assertEquals(
    xulComboToCanonicalCode({ modifiers: "accel", key: "T" }, true),
    "meta+KeyT",
    "mac accel becomes meta in canonical form",
  );
  assertEquals(
    xulComboToCanonicalCode({ modifiers: "accel", keycode: "VK_F5" }, false),
    "ctrl+F5",
    "keycode combos normalize into the code space",
  );
  assertEquals(
    xulComboToCanonicalCode(
      { modifiers: "accel", keycode: "VK_WHATEVER" },
      false,
    ),
    null,
    "unconvertible keycode yields null",
  );
  assertEquals(
    xulComboToCanonicalCode({ modifiers: "accel" }, false),
    null,
    "combo without key body yields null",
  );
}

function testFormatXulComboTextPerPlatform(): void {
  assertEquals(
    formatXulComboText({ modifiers: "accel,shift", key: "t" }, false),
    "Ctrl+Shift+T",
    "non-mac labels and uppercased key",
  );
  assertEquals(
    formatXulComboText({ modifiers: "accel", keycode: "VK_F5" }, false),
    "Ctrl+F5",
    "keycode stripped of its VK_ prefix",
  );
  assertEquals(
    formatXulComboText(
      { modifiers: "alt,control,meta,shift", key: "A" },
      false,
    ),
    "Ctrl+Alt+Shift+Meta+A",
    "non-mac labels follow the Ctrl/Alt/Shift/Meta order",
  );
  assertEquals(
    formatXulComboText({ modifiers: "meta", key: "Q" }, false),
    "Meta+Q",
    "non-mac meta label",
  );
  assertEquals(
    formatXulComboText({ modifiers: "", keycode: "VK_UP" }, false),
    "Up",
    "arrow keycodes are prettified",
  );
  assertEquals(
    formatXulComboText({ modifiers: "", keycode: "VK_BACK_SPACE" }, false),
    "Backspace",
    "multiword keycodes are prettified",
  );
  assertEquals(
    formatXulComboText({ modifiers: "", keycode: "VK_WHATEVER" }, false),
    "WHATEVER",
    "unknown keycodes keep their VK_-stripped name",
  );
  assertEquals(
    formatXulComboText({ modifiers: "accel,shift", key: "T" }, true),
    "Shift+Cmd+T",
    "mac accel renders as Cmd after Shift",
  );
  assertEquals(
    formatXulComboText({ modifiers: "control,alt", key: "A" }, true),
    "Ctrl+Opt+A",
    "mac control and alt render as Ctrl/Opt",
  );
  assertEquals(
    formatXulComboText({ modifiers: "accel", keycode: "VK_BACK_SPACE" }, true),
    "Cmd+Backspace",
    "mac keycode formatting",
  );
  assertEquals(
    formatXulComboText({ modifiers: "accel,control,meta", key: "H" }, true),
    "Ctrl+Cmd+H",
    "mac accel and meta collapse onto Cmd",
  );
  assertEquals(
    formatXulComboText({ modifiers: "", key: " " }, false),
    " ",
    "space key renders as a single space",
  );
  assertEquals(
    formatXulComboText({ modifiers: "", key: "" }, false),
    "",
    "empty combo renders as an empty string",
  );
  const emptyCombo: XulCombo = { modifiers: "os" };
  assertEquals(
    formatXulComboText(emptyCombo, false),
    "",
    "only-ignored-token combo renders as an empty string",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "modifiersToXulString maps per platform and sorts tokens",
      fn: testModifiersToXulStringPerPlatform,
    },
    {
      name: "xulStringToModifiers parses tokens platform-aware",
      fn: testXulStringToModifiersParsesPlatformAware,
    },
    {
      name: "modifier conversion round trips on both platforms",
      fn: testModifierConversionRoundTripsBothPlatforms,
    },
    {
      name: "codeToXulKey maps every supported event.code",
      fn: testCodeToXulKeyForwardMapping,
    },
    {
      name: "xulKeyToCode reverses and round trips code mappings",
      fn: testXulKeyToCodeReverseMapping,
    },
    {
      name: "unknown codes and keys return null",
      fn: testUnknownCodesAndKeysReturnNull,
    },
    {
      name: "canonical codes match across Floorp and XUL formats",
      fn: testCanonicalCodesMatchAcrossFormats,
    },
    {
      name: "formatXulComboText renders per platform",
      fn: testFormatXulComboTextPerPlatform,
    },
  ];
  await runTests("NRKeySyncCombo.test.ts", tests);
}
