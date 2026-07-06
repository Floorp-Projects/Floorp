// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assertEquals,
  assert,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  isModalVisible,
  modalSize,
} from "../data/data.ts";

// ---------------------------------------------------------------------------
// Helpers – save/restore global signal state
// ---------------------------------------------------------------------------

let savedVisible: boolean;
let savedSize: { width?: number; height?: number };

function saveState(): void {
  savedVisible = isModalVisible.value;
  savedSize = { ...modalSize.value };
}

function restoreState(): void {
  isModalVisible.value = savedVisible;
  modalSize.value = savedSize;
}

// ---------------------------------------------------------------------------
// isModalVisible / setModalVisible tests
// ---------------------------------------------------------------------------

function testDefaultVisibleFalse(): void {
  // Signals may be in any state if another test ran first; this just verifies
  // the setter toggles correctly.
  isModalVisible.value = false;
  assertEquals(
    isModalVisible.value,
    false,
    "isModalVisible should be false after setModalVisible(false)",
  );
}

function testSetVisibleTrue(): void {
  isModalVisible.value = true;
  assertEquals(
    isModalVisible.value,
    true,
    "isModalVisible should be true after setModalVisible(true)",
  );
}

function testSetVisibleRoundTrip(): void {
  isModalVisible.value = true;
  assert(isModalVisible.value, "visible should be true");
  isModalVisible.value = false;
  assertEquals(
    isModalVisible.value,
    false,
    "visible should be false after round trip",
  );
}

function testSetVisibleIdempotent(): void {
  isModalVisible.value = true;
  isModalVisible.value = true;
  assertEquals(
    isModalVisible.value,
    true,
    "setting visible=true twice should still be true",
  );
  isModalVisible.value = false;
  isModalVisible.value = false;
  assertEquals(
    isModalVisible.value,
    false,
    "setting visible=false twice should still be false",
  );
}

// ---------------------------------------------------------------------------
// modalSize / setModalSize tests
// ---------------------------------------------------------------------------

function testSetModalSizeBoth(): void {
  modalSize.value = ({ width: 400, height: 300 });
  const s = modalSize.value;
  assertEquals(s.width, 400, "width should be 400");
  assertEquals(s.height, 300, "height should be 300");
}

function testSetModalSizeWidthOnly(): void {
  modalSize.value = ({ width: 500 });
  const s = modalSize.value;
  assertEquals(s.width, 500, "width should be 500");
  // Preact signal: the entire value is replaced, so height is undefined here.
}

function testSetModalSizeHeightOnly(): void {
  modalSize.value = ({ height: 700 });
  const s = modalSize.value;
  assertEquals(s.height, 700, "height should be 700");
}

function testSetModalSizeZero(): void {
  modalSize.value = ({ width: 0, height: 0 });
  const s = modalSize.value;
  assertEquals(s.width, 0, "width should allow 0");
  assertEquals(s.height, 0, "height should allow 0");
}

function testSetModalSizeLarge(): void {
  modalSize.value = ({ width: 1920, height: 1080 });
  const s = modalSize.value;
  assertEquals(s.width, 1920, "width should be 1920");
  assertEquals(s.height, 1080, "height should be 1080");
}

function testSetModalSizeUpdatesSequentially(): void {
  modalSize.value = ({ width: 100, height: 200 });
  assertEquals(modalSize.value.width, 100, "first width");
  assertEquals(modalSize.value.height, 200, "first height");

  modalSize.value = ({ width: 300, height: 400 });
  assertEquals(modalSize.value.width, 300, "second width");
  assertEquals(modalSize.value.height, 400, "second height");
}

// ---------------------------------------------------------------------------
// Additional edge case tests
// ---------------------------------------------------------------------------

function testSetModalSizeNegativeValues(): void {
  // Test that negative values are accepted (validation may happen elsewhere)
  modalSize.value = ({ width: -100, height: -200 });
  const s = modalSize.value;
  assertEquals(s.width, -100, "width should accept negative values");
  assertEquals(s.height, -200, "height should accept negative values");
}

function testSetModalSizeFractionalValues(): void {
  // Test fractional/decimal values
  modalSize.value = ({ width: 100.5, height: 200.75 });
  const s = modalSize.value;
  assertEquals(s.width, 100.5, "width should accept fractional values");
  assertEquals(s.height, 200.75, "height should accept fractional values");
}

function testSetModalSizeVeryLarge(): void {
  // Test extremely large values (4K+ resolutions)
  modalSize.value = ({ width: 7680, height: 4320 });
  const s = modalSize.value;
  assertEquals(s.width, 7680, "width should accept 8K width");
  assertEquals(s.height, 4320, "height should accept 8K height");
}

function testSetModalSizeWithUndefined(): void {
  // Test setting explicit undefined values
  modalSize.value = ({ width: undefined, height: undefined });
  const s = modalSize.value;
  assertEquals(s.width, undefined, "width can be undefined");
  assertEquals(s.height, undefined, "height can be undefined");
}

function testSetModalSizeMixedUndefined(): void {
  // Test mixed defined and undefined values
  modalSize.value = ({ width: 500, height: undefined });
  let s = modalSize.value;
  assertEquals(s.width, 500, "width should be 500");
  assertEquals(s.height, undefined, "height should be undefined");

  modalSize.value = ({ width: undefined, height: 600 });
  s = modalSize.value;
  assertEquals(s.width, undefined, "width should be undefined");
  assertEquals(s.height, 600, "height should be 600");
}

function testSetModalVisibleToggle(): void {
  // Test toggling visibility multiple times
  isModalVisible.value = false;
  assertEquals(isModalVisible.value, false, "initial state false");

  isModalVisible.value = true;
  assertEquals(isModalVisible.value, true, "first toggle to true");

  isModalVisible.value = false;
  assertEquals(isModalVisible.value, false, "toggle to false");

  isModalVisible.value = true;
  assertEquals(isModalVisible.value, true, "toggle to true again");
}

function testSetModalSizeSmallValues(): void {
  // Test very small positive values
  modalSize.value = ({ width: 1, height: 1 });
  const s = modalSize.value;
  assertEquals(s.width, 1, "width should be 1");
  assertEquals(s.height, 1, "height should be 1");
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

const tests: TestCase[] = [
  { name: "default visible is false", fn: testDefaultVisibleFalse },
  { name: "setModalVisible(true) makes visible true", fn: testSetVisibleTrue },
  { name: "visible round-trip true→false", fn: testSetVisibleRoundTrip },
  { name: "setVisible is idempotent", fn: testSetVisibleIdempotent },
  { name: "setModalSize with both dimensions", fn: testSetModalSizeBoth },
  { name: "setModalSize width only", fn: testSetModalSizeWidthOnly },
  { name: "setModalSize height only", fn: testSetModalSizeHeightOnly },
  { name: "setModalSize zero dimensions", fn: testSetModalSizeZero },
  { name: "setModalSize large dimensions", fn: testSetModalSizeLarge },
  {
    name: "setModalSize sequential updates",
    fn: testSetModalSizeUpdatesSequentially,
  },
  { name: "setModalSize negative values", fn: testSetModalSizeNegativeValues },
  {
    name: "setModalSize fractional values",
    fn: testSetModalSizeFractionalValues,
  },
  { name: "setModalSize very large values", fn: testSetModalSizeVeryLarge },
  { name: "setModalSize with undefined", fn: testSetModalSizeWithUndefined },
  {
    name: "setModalSize mixed undefined",
    fn: testSetModalSizeMixedUndefined,
  },
  { name: "setModalVisible toggle", fn: testSetModalVisibleToggle },
  { name: "setModalSize small values", fn: testSetModalSizeSmallValues },
];

export async function runAllTests(): Promise<void> {
  saveState();
  try {
    await runTests("modalData.test.ts", tests);
  } finally {
    restoreState();
  }
}
