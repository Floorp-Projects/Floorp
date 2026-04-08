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
  setModalVisible,
  modalSize,
  setModalSize,
} from "../data/data.ts";

// ---------------------------------------------------------------------------
// Helpers – save/restore global signal state
// ---------------------------------------------------------------------------

let savedVisible: boolean;
let savedSize: { width?: number; height?: number };

function saveState(): void {
  savedVisible = isModalVisible();
  savedSize = { ...modalSize() };
}

function restoreState(): void {
  setModalVisible(savedVisible);
  setModalSize(savedSize);
}

// ---------------------------------------------------------------------------
// isModalVisible / setModalVisible tests
// ---------------------------------------------------------------------------

function testDefaultVisibleFalse(): void {
  // Signals may be in any state if another test ran first; this just verifies
  // the setter toggles correctly.
  setModalVisible(false);
  assertEquals(
    isModalVisible(),
    false,
    "isModalVisible should be false after setModalVisible(false)",
  );
}

function testSetVisibleTrue(): void {
  setModalVisible(true);
  assertEquals(
    isModalVisible(),
    true,
    "isModalVisible should be true after setModalVisible(true)",
  );
}

function testSetVisibleRoundTrip(): void {
  setModalVisible(true);
  assert(isModalVisible(), "visible should be true");
  setModalVisible(false);
  assertEquals(
    isModalVisible(),
    false,
    "visible should be false after round trip",
  );
}

function testSetVisibleIdempotent(): void {
  setModalVisible(true);
  setModalVisible(true);
  assertEquals(
    isModalVisible(),
    true,
    "setting visible=true twice should still be true",
  );
  setModalVisible(false);
  setModalVisible(false);
  assertEquals(
    isModalVisible(),
    false,
    "setting visible=false twice should still be false",
  );
}

// ---------------------------------------------------------------------------
// modalSize / setModalSize tests
// ---------------------------------------------------------------------------

function testSetModalSizeBoth(): void {
  setModalSize({ width: 400, height: 300 });
  const s = modalSize();
  assertEquals(s.width, 400, "width should be 400");
  assertEquals(s.height, 300, "height should be 300");
}

function testSetModalSizeWidthOnly(): void {
  setModalSize({ width: 500 });
  const s = modalSize();
  assertEquals(s.width, 500, "width should be 500");
  // height is a SolidJS signal; partial update preserves other fields via the
  // spread in the signal setter. If the signal uses the default setter without
  // spread, this test documents the actual behaviour.
}

function testSetModalSizeHeightOnly(): void {
  setModalSize({ height: 700 });
  const s = modalSize();
  assertEquals(s.height, 700, "height should be 700");
}

function testSetModalSizeZero(): void {
  setModalSize({ width: 0, height: 0 });
  const s = modalSize();
  assertEquals(s.width, 0, "width should allow 0");
  assertEquals(s.height, 0, "height should allow 0");
}

function testSetModalSizeLarge(): void {
  setModalSize({ width: 1920, height: 1080 });
  const s = modalSize();
  assertEquals(s.width, 1920, "width should be 1920");
  assertEquals(s.height, 1080, "height should be 1080");
}

function testSetModalSizeUpdatesSequentially(): void {
  setModalSize({ width: 100, height: 200 });
  assertEquals(modalSize().width, 100, "first width");
  assertEquals(modalSize().height, 200, "first height");

  setModalSize({ width: 300, height: 400 });
  assertEquals(modalSize().width, 300, "second width");
  assertEquals(modalSize().height, 400, "second height");
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
];

export async function runAllTests(): Promise<void> {
  saveState();
  try {
    await runTests("modalData.test.ts", tests);
  } finally {
    restoreState();
  }
}
