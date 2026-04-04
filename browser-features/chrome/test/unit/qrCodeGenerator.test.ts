// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { QRCodeManager } from "../../common/qr-code-generator/qr-code-manager.tsx";

type TestCase = { name: string; fn: () => void | Promise<void> };

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected)
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
}

function testQRCodeManagerConstruction(): void {
  const manager = new QRCodeManager();
  assert(manager !== null, "QRCodeManager should be constructable");
  assert(typeof manager.showPanel === "function", "showPanel should be a signal accessor");
  assert(typeof manager.setShowPanel === "function", "setShowPanel should be a signal setter");
  assert(typeof manager.currentUrl === "function", "currentUrl should be a signal accessor");
  assert(typeof manager.setCurrentUrl === "function", "setCurrentUrl should be a signal setter");
}

function testShowPanelDefaultIsFalse(): void {
  const manager = new QRCodeManager();
  assertEquals(manager.showPanel(), false, "showPanel should default to false");
}

function testCurrentUrlDefaultIsEmpty(): void {
  const manager = new QRCodeManager();
  assertEquals(manager.currentUrl(), "", "currentUrl should default to empty string");
}

function testShowQRPanel(): void {
  const manager = new QRCodeManager();
  manager.showQRPanel();
  assertEquals(manager.showPanel(), true, "showPanel should be true after showQRPanel()");
}

function testHideQRPanel(): void {
  const manager = new QRCodeManager();
  manager.showQRPanel();
  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "showPanel should be false after hideQRPanel()");
}

function testSetCurrentUrlManually(): void {
  const manager = new QRCodeManager();
  manager.setCurrentUrl("https://example.com");
  assertEquals(
    manager.currentUrl(),
    "https://example.com",
    "currentUrl should reflect manual set",
  );
}

function testGlobalFloorpQrCodeBinding(): void {
  const _manager = new QRCodeManager();
  assert(
    globalThis.gFloorp?.qrCode !== undefined,
    "gFloorp.qrCode should be defined after construction",
  );
  assert(
    typeof globalThis.gFloorp.qrCode.show === "function",
    "gFloorp.qrCode.show should be a function",
  );
  assert(
    typeof globalThis.gFloorp.qrCode.hide === "function",
    "gFloorp.qrCode.hide should be a function",
  );
  assert(
    typeof globalThis.gFloorp.qrCode.generateForUrl === "function",
    "gFloorp.qrCode.generateForUrl should be a function",
  );
}

function testGlobalFloorpPageActionBinding(): void {
  const _manager = new QRCodeManager();
  assert(
    globalThis.gFloorpPageAction?.qrCode !== undefined,
    "gFloorpPageAction.qrCode should be defined after construction",
  );
  assert(
    typeof globalThis.gFloorpPageAction.qrCode.onPopupShowing === "function",
    "gFloorpPageAction.qrCode.onPopupShowing should be a function",
  );
}

function testUpdateCurrentTabUrl(): void {
  const manager = new QRCodeManager();
  // updateCurrentTabUrl reads from gBrowser.selectedTab; it should not throw
  let threw = false;
  try {
    manager.updateCurrentTabUrl();
  } catch {
    threw = true;
  }
  assert(!threw, "updateCurrentTabUrl should not throw");

  // If gBrowser is available and has a selected tab, currentUrl should be populated
  if (globalThis.gBrowser?.selectedTab?.linkedBrowser?.currentURI?.spec) {
    const url = manager.currentUrl();
    assert(url.length > 0, "currentUrl should be non-empty when a tab is selected");
  }
}

function testDownloadQRCodeWithoutGenerating(): void {
  const manager = new QRCodeManager();
  // downloadQRCode should not throw even when no QR code has been generated
  let threw = false;
  try {
    manager.downloadQRCode("png");
  } catch {
    threw = true;
  }
  assert(!threw, "downloadQRCode should not throw when no QR code is generated");
}

function testInitDoesNotThrow(): void {
  const manager = new QRCodeManager();
  let threw = false;
  try {
    manager.init();
  } catch {
    threw = true;
  }
  assert(!threw, "init() should not throw");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "QRCodeManager construction", fn: testQRCodeManagerConstruction },
    { name: "showPanel default is false", fn: testShowPanelDefaultIsFalse },
    { name: "currentUrl default is empty", fn: testCurrentUrlDefaultIsEmpty },
    { name: "showQRPanel sets showPanel true", fn: testShowQRPanel },
    { name: "hideQRPanel sets showPanel false", fn: testHideQRPanel },
    { name: "setCurrentUrl works manually", fn: testSetCurrentUrlManually },
    { name: "gFloorp.qrCode binding", fn: testGlobalFloorpQrCodeBinding },
    { name: "gFloorpPageAction.qrCode binding", fn: testGlobalFloorpPageActionBinding },
    { name: "updateCurrentTabUrl does not throw", fn: testUpdateCurrentTabUrl },
    { name: "downloadQRCode without generating", fn: testDownloadQRCodeWithoutGenerating },
    { name: "init does not throw", fn: testInitDoesNotThrow },
  ];

  const failures: string[] = [];
  for (const test of tests) {
    try {
      await test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }
  if (failures.length > 0)
    throw new Error(`qrCodeGenerator.test.ts failures: ${failures.join(" | ")}`);
}
