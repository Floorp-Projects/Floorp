// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  filterWorkspaceIconPickerOptions,
  getSafeWorkspaceIconPickerOptions,
  resolveWorkspaceIconPickerDisplayValue,
  selectWorkspaceIconPickerValue,
} from "../src/workspaceIconPickerModel.ts";
import type { TFormOption } from "../../chrome/common/modal-parent/utils/type.ts";
import {
  WORKSPACE_ICON_NO_CHANGE_SENTINEL,
} from "../../chrome/common/workspaces/workspace-modal.tsx";
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";

const SAFE_PREVIEW = "data:image/svg+xml;base64,PHN2Zz48L3N2Zz4=";
const options: TFormOption[] = [
  {
    label: "Article",
    value: "floorp-icon:v1:article",
    icon: SAFE_PREVIEW,
    keywords: ["document", "news"],
  },
  {
    label: "Music",
    value: "floorp-icon:v1:music",
    icon: SAFE_PREVIEW,
    keywords: ["audio", "song"],
  },
];

function testSafeOptionsRejectNetworkAndMalformedValues(): void {
  const filtered = getSafeWorkspaceIconPickerOptions([
    ...options,
    { ...options[0], icon: "https://example.invalid/icon.svg" },
    { ...options[0], value: "article" },
    { ...options[0], value: "floorp-icon:v1:Article" },
    { ...options[0], value: "floorp-icon:v1:article " },
    { ...options[0], icon: "data:text/html;base64,PHNjcmlwdD4=" },
  ]);
  assertEquals(
    filtered.length,
    2,
    "only unique canonical bundled options remain",
  );
  assert(
    filtered.every((option) => option.icon === SAFE_PREVIEW),
    "no network or non-SVG preview survives",
  );
}

function testFilterUsesLabelSlugAndKeywords(): void {
  assertEquals(
    filterWorkspaceIconPickerOptions(options, "MUSIC").length,
    1,
    "label",
  );
  assertEquals(
    filterWorkspaceIconPickerOptions(options, "article").length,
    1,
    "slug",
  );
  assertEquals(
    filterWorkspaceIconPickerOptions(options, "document").length,
    1,
    "keyword",
  );
  assertEquals(
    filterWorkspaceIconPickerOptions(options, "missing").length,
    0,
    "miss",
  );
  assertEquals(
    filterWorkspaceIconPickerOptions(options, "  ").length,
    2,
    "empty query",
  );
}

function testNoChangeAndInvalidSelectionPreserveSentinel(): void {
  const sentinel = WORKSPACE_ICON_NO_CHANGE_SENTINEL;
  assertEquals(
    selectWorkspaceIconPickerValue(sentinel, "article", options),
    sentinel,
    "alias cannot be emitted",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(
      sentinel,
      "https://example.invalid/icon.svg",
      options,
    ),
    sentinel,
    "URI cannot be emitted",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(sentinel, "floorp-icon:v1:music", options),
    "floorp-icon:v1:music",
    "explicit listed canonical selection is emitted",
  );
}

function testDisplayValueDoesNotChangeFormValue(): void {
  const sentinel = WORKSPACE_ICON_NO_CHANGE_SENTINEL;
  assertEquals(
    resolveWorkspaceIconPickerDisplayValue(
      sentinel,
      "floorp-icon:v1:article",
      options,
    ),
    "floorp-icon:v1:article",
    "safe initial display is independent of sentinel",
  );
  assertEquals(
    resolveWorkspaceIconPickerDisplayValue(
      "floorp-icon:v1:music",
      "floorp-icon:v1:article",
      options,
    ),
    "floorp-icon:v1:music",
    "explicit selection becomes display value",
  );
  assertEquals(
    resolveWorkspaceIconPickerDisplayValue(sentinel, "opaque", options),
    null,
    "opaque initial value is never displayed",
  );
}

function testDisplayedFallbackCanBeExplicitlyCanonicalized(): void {
  const sentinel = WORKSPACE_ICON_NO_CHANGE_SENTINEL;
  const displayValue = resolveWorkspaceIconPickerDisplayValue(
    sentinel,
    "floorp-icon:v1:article",
    options,
  );
  assertEquals(
    displayValue,
    "floorp-icon:v1:article",
    "safe fallback is displayed without changing the sentinel",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(sentinel, displayValue, options),
    "floorp-icon:v1:article",
    "activating the displayed fallback emits its canonical value",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(sentinel, "article", options),
    sentinel,
    "fallback activation still rejects an alias",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(
      sentinel,
      "floorp-icon:v1:unknown",
      options,
    ),
    sentinel,
    "fallback activation still rejects an unknown canonical-looking value",
  );
  assertEquals(
    selectWorkspaceIconPickerValue(
      sentinel,
      "https://example.invalid/icon.svg",
      options,
    ),
    sentinel,
    "fallback activation still rejects a URI",
  );
}

export function runAllTests(): void {
  const tests: TestCase[] = [
    {
      name: "safe options reject network and malformed values",
      fn: testSafeOptionsRejectNetworkAndMalformedValues,
    },
    {
      name: "filter uses label slug and keywords",
      fn: testFilterUsesLabelSlugAndKeywords,
    },
    {
      name: "no-change and invalid selection preserve sentinel",
      fn: testNoChangeAndInvalidSelectionPreserveSentinel,
    },
    {
      name: "display value does not change form value",
      fn: testDisplayValueDoesNotChangeFormValue,
    },
    {
      name: "displayed fallback can be explicitly canonicalized",
      fn: testDisplayedFallbackCanBeExplicitlyCanonicalized,
    },
  ];
  const failures: string[] = [];
  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      failures.push(
        `${test.name}: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  }
  if (failures.length > 0) {
    throw new Error(
      `workspaceIconPickerModel.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
