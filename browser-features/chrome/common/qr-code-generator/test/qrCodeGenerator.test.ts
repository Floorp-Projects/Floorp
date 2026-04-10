// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { QRCodeManager } from "../qr-code-manager.tsx";

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function testQRCodeManagerConstruction(): void {
  const manager = new QRCodeManager();
  assert(manager !== null, "QRCodeManager should be constructable");
  assert(
    typeof manager.showPanel === "function",
    "showPanel should be a signal accessor",
  );
  assert(
    typeof manager.setShowPanel === "function",
    "setShowPanel should be a signal setter",
  );
  assert(
    typeof manager.currentUrl === "function",
    "currentUrl should be a signal accessor",
  );
  assert(
    typeof manager.setCurrentUrl === "function",
    "setCurrentUrl should be a signal setter",
  );
}

function testShowPanelDefaultIsFalse(): void {
  const manager = new QRCodeManager();
  assertEquals(manager.showPanel(), false, "showPanel should default to false");
}

function testCurrentUrlDefaultIsEmpty(): void {
  const manager = new QRCodeManager();
  assertEquals(
    manager.currentUrl(),
    "",
    "currentUrl should default to empty string",
  );
}

function testShowQRPanel(): void {
  const manager = new QRCodeManager();
  manager.showQRPanel();
  assertEquals(
    manager.showPanel(),
    true,
    "showPanel should be true after showQRPanel()",
  );
}

function testHideQRPanel(): void {
  const manager = new QRCodeManager();
  manager.showQRPanel();
  manager.hideQRPanel();
  assertEquals(
    manager.showPanel(),
    false,
    "showPanel should be false after hideQRPanel()",
  );
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
  const g = globalThis as Record<string, unknown>;
  const floorp = g.gFloorp as Record<string, unknown> | undefined;
  const qrCode = floorp?.qrCode as Record<string, unknown> | undefined;
  assert(
    qrCode !== undefined,
    "gFloorp.qrCode should be defined after construction",
  );
  assert(
    typeof qrCode?.show === "function",
    "gFloorp.qrCode.show should be a function",
  );
  assert(
    typeof qrCode?.hide === "function",
    "gFloorp.qrCode.hide should be a function",
  );
  assert(
    typeof qrCode?.generateForUrl === "function",
    "gFloorp.qrCode.generateForUrl should be a function",
  );
}

function testGlobalFloorpPageActionBinding(): void {
  const _manager = new QRCodeManager();
  const g = globalThis as Record<string, unknown>;
  const pageAction = g.gFloorpPageAction as Record<string, unknown> | undefined;
  const qrCode = pageAction?.qrCode as Record<string, unknown> | undefined;
  assert(
    qrCode !== undefined,
    "gFloorpPageAction.qrCode should be defined after construction",
  );
  assert(
    typeof qrCode?.onPopupShowing === "function",
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
  if ((globalThis as Record<string, unknown>).gBrowser && ((globalThis as Record<string, unknown>).gBrowser as Record<string, unknown>)?.selectedTab) {
    const selectedTab = ((globalThis as Record<string, unknown>).gBrowser as Record<string, unknown>).selectedTab as unknown as { linkedBrowser?: { currentURI?: { spec?: string } } };
    if (selectedTab?.linkedBrowser?.currentURI?.spec) {
      const url = manager.currentUrl();
      assert(
        url.length > 0,
        "currentUrl should be non-empty when a tab is selected",
      );
    }
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
  assert(
    !threw,
    "downloadQRCode should not throw when no QR code is generated",
  );
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

function testSetShowPanelDirectly(): void {
  const manager = new QRCodeManager();
  manager.setShowPanel(true);
  assertEquals(manager.showPanel(), true, "setShowPanel(true) should work");

  manager.setShowPanel(false);
  assertEquals(manager.showPanel(), false, "setShowPanel(false) should work");
}

function testGenerateForUrlWithInvalidUrl(): void {
  new QRCodeManager();

  // Test with invalid URLs (should not crash)
  const invalidUrls = [
    "",
    "not-a-url",
    "javascript:alert('xss')",
    "data:text/html,<script>alert('xss')</script>",
    "about:blank",
    "chrome://browser/content/browser.xul",
  ];

  let threw = false;
  try {
    invalidUrls.forEach((url) => {
      const floorp = (globalThis as Record<string, unknown>).gFloorp as Record<string, unknown> | undefined;
      const qrCode = floorp?.qrCode as Record<string, unknown> | undefined;
      if (typeof qrCode?.generateForUrl === "function") {
        (qrCode.generateForUrl as (url: string) => void)(url);
      }
    });
  } catch {
    threw = true;
  }
  assert(!threw, "generateForUrl should not throw for invalid URLs");
}

function testManagerWithSpecialCharactersInUrl(): void {
  const manager = new QRCodeManager();

  const specialUrls = [
    "https://example.com?param=value&other=123",
    "https://example.com/path/with spaces",
    "https://example.com/path/with/unicode/テスト",
    "https://example.com:8080/path",
    "https://user:pass@example.com/path",
  ];

  let threw = false;
  try {
    specialUrls.forEach((url) => {
      manager.setCurrentUrl(url);
      assertEquals(manager.currentUrl(), url, `URL ${url} should be set correctly`);
    });
  } catch {
    threw = true;
  }
  assert(!threw, "special characters in URLs should be handled");
}

function testSignalReactivity(): void {
  const manager = new QRCodeManager();

  // Test that signals are reactive
  const url1 = "https://example.com/1";
  const url2 = "https://example.com/2";

  manager.setCurrentUrl(url1);
  assertEquals(manager.currentUrl(), url1, "currentUrl should update to url1");

  manager.setCurrentUrl(url2);
  assertEquals(manager.currentUrl(), url2, "currentUrl should update to url2");

  manager.setShowPanel(true);
  assertEquals(manager.showPanel(), true, "showPanel should be true");

  manager.setShowPanel(false);
  assertEquals(manager.showPanel(), false, "showPanel should be false");
}

function testGlobalBindingsAfterMultipleInstances(): void {
  // Create multiple managers
  const _manager1 = new QRCodeManager();
  const _manager2 = new QRCodeManager();

  // Global bindings should still be defined
  const g = globalThis as Record<string, unknown>;
  assert(
    (g.gFloorp as Record<string, unknown> | undefined)?.qrCode !== undefined,
    "gFloorp.qrCode should be defined",
  );
  assert(
    (g.gFloorpPageAction as Record<string, unknown> | undefined)?.qrCode !== undefined,
    "gFloorpPageAction.qrCode should be defined",
  );

  // Methods should be callable
  let threw = false;
  try {
    const qrCode = (g.gFloorp as Record<string, unknown>)?.qrCode as Record<string, unknown> | undefined;
    if (typeof qrCode?.show === "function") (qrCode.show as () => void)();
    if (typeof qrCode?.hide === "function") (qrCode.hide as () => void)();
    const pageActionQrCode = (g.gFloorpPageAction as Record<string, unknown>)?.qrCode as Record<string, unknown> | undefined;
    if (typeof pageActionQrCode?.onPopupShowing === "function") (pageActionQrCode.onPopupShowing as () => void)();
  } catch {
    threw = true;
  }
  assert(!threw, "global methods should remain callable");
}

function testManagerInitialState(): void {
  const manager = new QRCodeManager();

  // Verify all initial states
  assertEquals(manager.showPanel(), false, "showPanel should be false initially");
  assertEquals(manager.currentUrl(), "", "currentUrl should be empty initially");
  assertEquals(typeof manager.showPanel, "function", "showPanel should be a function");
  assertEquals(typeof manager.setShowPanel, "function", "setShowPanel should be a function");
  assertEquals(typeof manager.currentUrl, "function", "currentUrl should be a function");
  assertEquals(typeof manager.setCurrentUrl, "function", "setCurrentUrl should be a function");
}

function testUrlWithEmptyString(): void {
  const manager = new QRCodeManager();

  manager.setCurrentUrl("https://example.com");
  assertEquals(manager.currentUrl(), "https://example.com", "URL should be set");

  manager.setCurrentUrl("");
  assertEquals(manager.currentUrl(), "", "URL should be cleared");
}

function testShowPanelMultipleTimes(): void {
  const manager = new QRCodeManager();

  // Calling showQRPanel multiple times should not cause issues
  manager.showQRPanel();
  assertEquals(manager.showPanel(), true, "showPanel should be true");

  manager.showQRPanel();
  assertEquals(manager.showPanel(), true, "showPanel should remain true");

  manager.showQRPanel();
  assertEquals(manager.showPanel(), true, "showPanel should still be true");
}

function testHidePanelMultipleTimes(): void {
  const manager = new QRCodeManager();

  manager.showQRPanel();
  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "showPanel should be false");

  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "showPanel should remain false");

  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "showPanel should still be false");
}

function testUpdateCurrentTabUrlWithMissingBrowserProperties(): void {
  const manager = new QRCodeManager();

  // Save original gBrowser
  const originalGBrowser = globalThis.gBrowser;

  try {
    // Create a gBrowser with missing properties
    globalThis.gBrowser = {
      selectedTab: {},
      tabContainer: {
        addEventListener: () => {},
      },
    } as unknown as typeof globalThis.gBrowser;

    // Should not throw
    let threw = false;
    try {
      manager.updateCurrentTabUrl();
      manager.init(); // This also accesses gBrowser
    } catch {
      threw = true;
    }
    assert(!threw, "should handle missing browser properties gracefully");
  } finally {
    // Restore original gBrowser
    if (originalGBrowser) {
      globalThis.gBrowser = originalGBrowser;
    } else {
      (globalThis as { gBrowser?: unknown }).gBrowser = undefined;
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("qrCodeGenerator.test.ts", [
    { name: "QRCodeManager construction", fn: testQRCodeManagerConstruction },
    { name: "showPanel default is false", fn: testShowPanelDefaultIsFalse },
    { name: "currentUrl default is empty", fn: testCurrentUrlDefaultIsEmpty },
    { name: "showQRPanel sets showPanel true", fn: testShowQRPanel },
    { name: "hideQRPanel sets showPanel false", fn: testHideQRPanel },
    { name: "setCurrentUrl works manually", fn: testSetCurrentUrlManually },
    { name: "gFloorp.qrCode binding", fn: testGlobalFloorpQrCodeBinding },
    {
      name: "gFloorpPageAction.qrCode binding",
      fn: testGlobalFloorpPageActionBinding,
    },
    { name: "updateCurrentTabUrl does not throw", fn: testUpdateCurrentTabUrl },
    {
      name: "downloadQRCode without generating",
      fn: testDownloadQRCodeWithoutGenerating,
    },
    { name: "init does not throw", fn: testInitDoesNotThrow },
    { name: "setShowPanel directly", fn: testSetShowPanelDirectly },
    { name: "generateForUrl with invalid URL", fn: testGenerateForUrlWithInvalidUrl },
    { name: "manager with special characters in URL", fn: testManagerWithSpecialCharactersInUrl },
    { name: "signal reactivity", fn: testSignalReactivity },
    { name: "global bindings after multiple instances", fn: testGlobalBindingsAfterMultipleInstances },
    { name: "manager initial state", fn: testManagerInitialState },
    { name: "URL with empty string", fn: testUrlWithEmptyString },
    { name: "showPanel multiple times", fn: testShowPanelMultipleTimes },
    { name: "hidePanel multiple times", fn: testHidePanelMultipleTimes },
    { name: "updateCurrentTabUrl with missing browser properties", fn: testUpdateCurrentTabUrlWithMissingBrowserProperties },
  ]);
}
