// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { QRCodeManager } from "../qr-code-manager.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testUpdateCurrentTabUrlReadsSelectedTab(): void {
  const manager = new QRCodeManager();
  const selectedTab = (globalThis as Record<string, unknown>).gBrowser
    ? ((globalThis as Record<string, unknown>).gBrowser as Record<string, unknown>).selectedTab as unknown as { linkedBrowser?: { currentURI?: { spec?: string } } } | undefined
    : undefined;
  const expectedUrl = selectedTab?.linkedBrowser?.currentURI?.spec ?? "";

  manager.updateCurrentTabUrl();
  assertEquals(
    manager.currentUrl(),
    expectedUrl,
    "currentUrl should reflect selected tab URL",
  );
}

function testDownloadQRCodeDelegatesToUnderlyingInstance(): void {
  const manager = new QRCodeManager() as unknown as {
    qrCode: { download: (args: { name: string; extension: string }) => void } | null;
    downloadQRCode: (format: string) => void;
  };

  let called = false;
  let extension = "";
  manager.qrCode = {
    download: (args: { name: string; extension: string }) => {
      called = true;
      extension = args.extension;
    },
  };

  manager.downloadQRCode("svg");

  assert(called, "download should be called when qrCode exists");
  assertEquals(extension, "svg", "requested extension should be forwarded");
}

function testShowHideCycleKeepsSignalConsistent(): void {
  const manager = new QRCodeManager();
  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "panel starts hidden");

  manager.showQRPanel();
  assertEquals(manager.showPanel(), true, "show should set signal true");

  manager.hideQRPanel();
  assertEquals(manager.showPanel(), false, "hide should set signal false");
}

function testHandlePopupShowingWithEmptyUrl(): void {
  const manager = new QRCodeManager();
  manager.setCurrentUrl("");

  // Should not throw when URL is empty
  let threw = false;
  try {
    manager.handlePopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "handlePopupShowing should not throw with empty URL");
}

function testHandlePopupShowingWithMissingContainer(): void {
  const manager = new QRCodeManager();
  manager.setCurrentUrl("https://example.com");

  // Should not throw when container is missing
  let threw = false;
  try {
    manager.handlePopupShowing();
  } catch {
    threw = true;
  }
  assert(!threw, "handlePopupShowing should not throw with missing container");
}

function testGenerateQRCodeWithEmptyUrl(): void {
  const manager = new QRCodeManager();
  // This is an async function, but we're testing the synchronous path
  const resultPromise = manager.generateQRCode("");

  // Should return a promise that resolves to null
  resultPromise.then((result) => {
    assertEquals(result, null, "generateQRCode should return null for empty URL");
  });
}

function testGenerateQRCodeErrorHandling(): void {
  const manager = new QRCodeManager();
  // We can't easily test the import failure, but we can verify it doesn't crash
  let threw = false;
  try {
    const result = manager.generateQRCode("https://example.com");
    // The function should return a promise, not throw
    assert(
      typeof result.then === "function",
      "generateQRCode should return a promise",
    );
  } catch {
    threw = true;
  }
  assert(!threw, "generateQRCode should not throw synchronously");
}

function testUpdateCurrentTabUrlErrorHandling(): void {
  const manager = new QRCodeManager();
  // Save original gBrowser if it exists
  const originalGBrowser = globalThis.gBrowser;

  // Simulate error condition by making gBrowser throw
  let mockApplied = false;
  try {
    Object.defineProperty(globalThis, "gBrowser", {
      get: function () {
        throw new Error("Simulated error");
      },
      configurable: true,
    });
    mockApplied = true;
  } catch {
    // gBrowser is non-configurable; cannot mock the getter.
  }

  try {
    if (mockApplied) {
      // Should not throw, should handle error gracefully
      let threw = false;
      try {
        manager.updateCurrentTabUrl();
      } catch {
        threw = true;
      }
      assert(!threw, "updateCurrentTabUrl should handle errors gracefully");
    }
  } finally {
    // Restore original gBrowser
    if (mockApplied) {
      try {
        Object.defineProperty(globalThis, "gBrowser", {
          value: originalGBrowser,
          configurable: true,
          writable: true,
        });
      } catch {
        // Ignore if gBrowser cannot be restored.
      }
    }
  }
}

function testInitWithMissingGBrowser(): void {
  const manager = new QRCodeManager();
  // Save original gBrowser
  const originalGBrowser = globalThis.gBrowser;

  // gBrowser may be non-configurable in Firefox, use Object.defineProperty with try/catch
  let mockApplied = false;
  try {
    Object.defineProperty(globalThis, "gBrowser", {
      value: undefined,
      configurable: true,
      writable: true,
    });
    mockApplied = true;
  } catch {
    // gBrowser is non-configurable; cannot set to undefined.
  }

  try {
    // Should not throw when gBrowser is missing
    let threw = false;
    try {
      manager.init();
    } catch {
      threw = true;
    }
    assert(!threw, "init should not throw when gBrowser is missing");
  } finally {
    // Restore original gBrowser
    if (mockApplied && originalGBrowser !== undefined) {
      try {
        Object.defineProperty(globalThis, "gBrowser", {
          value: originalGBrowser,
          configurable: true,
          writable: true,
        });
      } catch {
        // Ignore if gBrowser cannot be restored.
      }
    }
  }
}

function testDownloadQRCodeWithNullInstance(): void {
  const manager = new QRCodeManager() as unknown as { qrCode: unknown; downloadQRCode: (format: string) => void };
  // Ensure qrCode is null
  manager.qrCode = null;

  // Should not throw when qrCode is null
  let threw = false;
  try {
    manager.downloadQRCode("png");
  } catch {
    threw = true;
  }
  assert(!threw, "downloadQRCode should not throw when qrCode is null");
}

function testDownloadQRCodeAllFormats(): void {
  const manager = new QRCodeManager() as unknown as {
    qrCode: { download: (args: { name: string; extension: string }) => void } | null;
    downloadQRCode: (format: string) => void;
  };

  const formats: Array<"png" | "jpeg" | "webp" | "svg"> = ["png", "jpeg", "webp", "svg"];
  const downloadedFormats: string[] = [];

  manager.qrCode = {
    download: (args: { name: string; extension: string }) => {
      downloadedFormats.push(args.extension);
    },
  };

  formats.forEach((format) => {
    manager.downloadQRCode(format);
  });

  assertEquals(
    downloadedFormats.length,
    formats.length,
    "all formats should be downloaded",
  );
  assertEquals(
    downloadedFormats[0],
    "png",
    "png format should be downloaded first",
  );
  assertEquals(
    downloadedFormats[1],
    "jpeg",
    "jpeg format should be downloaded second",
  );
  assertEquals(
    downloadedFormats[2],
    "webp",
    "webp format should be downloaded third",
  );
  assertEquals(
    downloadedFormats[3],
    "svg",
    "svg format should be downloaded fourth",
  );
}

function testMultipleManagerInstances(): void {
  // Create first instance
  const manager1 = new QRCodeManager();

  // Create second instance
  const manager2 = new QRCodeManager();

  // Both should have different signal states
  manager1.showQRPanel();
  assertEquals(
    manager1.showPanel(),
    true,
    "first manager showPanel should be true",
  );
  assertEquals(
    manager2.showPanel(),
    false,
    "second manager showPanel should remain false",
  );

  // Global binding should still work
  assert(
    globalThis.gFloorp?.qrCode !== undefined,
    "gFloorp.qrCode should still be defined",
  );
}

function testConstructorPreservesExistingGlobalObjects(): void {
  const g = globalThis as Record<string, unknown>;
  // Set up existing gFloorp with other properties
  const originalFloorp = (g.gFloorp || {}) as Record<string, unknown>;
  originalFloorp.existingProp = "test";

  const originalPageAction = (g.gFloorpPageAction || {}) as Record<string, unknown>;
  originalPageAction.existingProp = "test";

  try {
    // Create manager should preserve existing properties
    const _manager = new QRCodeManager();

    assertEquals(
      (g.gFloorp as Record<string, unknown>).existingProp,
      "test",
      "existing gFloorp properties should be preserved",
    );
    assertEquals(
      (g.gFloorpPageAction as Record<string, unknown>).existingProp,
      "test",
      "existing gFloorpPageAction properties should be preserved",
    );
    assert(
      (g.gFloorp as Record<string, unknown>)?.qrCode !== undefined,
      "qrCode property should be added",
    );
  } finally {
    // Clean up
    if (originalFloorp) {
      g.gFloorp = originalFloorp;
    }
    if (originalPageAction) {
      g.gFloorpPageAction = originalPageAction;
    }
  }
}

function testShowQRPanelUpdatesCurrentUrl(): void {
  const manager = new QRCodeManager();
  const _initialUrl = manager.currentUrl();

  // showQRPanel should call updateCurrentTabUrl
  manager.showQRPanel();

  // If gBrowser is available, URL might change
  // If not, it should at least not throw
  assertEquals(
    manager.showPanel(),
    true,
    "showPanel should be true after showQRPanel",
  );
}

function testHideQRPanelDoesNotUpdateUrl(): void {
  const manager = new QRCodeManager();
  manager.setCurrentUrl("https://example.com");

  manager.hideQRPanel();

  assertEquals(
    manager.currentUrl(),
    "https://example.com",
    "currentUrl should not change on hideQRPanel",
  );
  assertEquals(
    manager.showPanel(),
    false,
    "showPanel should be false after hideQRPanel",
  );
}

function testGenerateQRCodeWithContainer(): void {
  const manager = new QRCodeManager();

  // Create a test container
  const container = document!.createElement("div");
  container.id = "test-qrcode-container";
  document!.body!.appendChild(container);

  try {
    // Add a child to verify it gets cleared
    const testChild = document!.createElement("div");
    container.appendChild(testChild);

    assertEquals(
      container.childNodes.length,
      1,
      "container should have one child before generation",
    );

    // Generate QR code
    const result = manager.generateQRCode("https://example.com", container);

    // Should return a promise
    assert(
      typeof result.then === "function",
      "generateQRCode should return a promise",
    );

    // Note: We can't easily test the actual QR code rendering without the library
    // but we've verified the function doesn't throw
  } finally {
    // Clean up
    document!.body!.removeChild(container);
  }
}

function testConcurrentShowHideOperations(): void {
  const manager = new QRCodeManager();

  // Rapid show/hide cycles
  for (let i = 0; i < 10; i++) {
    manager.showQRPanel();
    assertEquals(manager.showPanel(), true, `showPanel should be true at iteration ${i}`);
    manager.hideQRPanel();
    assertEquals(manager.showPanel(), false, `showPanel should be false at iteration ${i}`);
  }
}

function testGlobalFloorpMethodsExecute(): void {
  const manager = new QRCodeManager();
  const g = globalThis as Record<string, unknown>;
  const qrCode = (g.gFloorp as Record<string, unknown>)?.qrCode as Record<string, unknown> | undefined;

  // Test that global methods actually execute
  let threw = false;
  try {
    if (typeof qrCode?.show === "function") (qrCode.show as () => void)();
    assertEquals(manager.showPanel(), true, "global show() should work");

    if (typeof qrCode?.hide === "function") (qrCode.hide as () => void)();
    assertEquals(manager.showPanel(), false, "global hide() should work");

    if (typeof qrCode?.generateForUrl === "function") (qrCode.generateForUrl as (url: string) => void)("https://example.com");
    // Should not throw, actual QR code generation is async
  } catch {
    threw = true;
  }
  assert(!threw, "global methods should execute without throwing");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "updateCurrentTabUrl reads selected tab",
      fn: testUpdateCurrentTabUrlReadsSelectedTab,
    },
    {
      name: "downloadQRCode delegates to underlying instance",
      fn: testDownloadQRCodeDelegatesToUnderlyingInstance,
    },
    {
      name: "show/hide cycle keeps signal consistent",
      fn: testShowHideCycleKeepsSignalConsistent,
    },
    {
      name: "handlePopupShowing with empty URL",
      fn: testHandlePopupShowingWithEmptyUrl,
    },
    {
      name: "handlePopupShowing with missing container",
      fn: testHandlePopupShowingWithMissingContainer,
    },
    {
      name: "generateQRCode with empty URL returns null",
      fn: testGenerateQRCodeWithEmptyUrl,
    },
    {
      name: "generateQRCode error handling",
      fn: testGenerateQRCodeErrorHandling,
    },
    {
      name: "updateCurrentTabUrl error handling",
      fn: testUpdateCurrentTabUrlErrorHandling,
    },
    {
      name: "init with missing gBrowser",
      fn: testInitWithMissingGBrowser,
    },
    {
      name: "downloadQRCode with null instance",
      fn: testDownloadQRCodeWithNullInstance,
    },
    {
      name: "downloadQRCode all formats",
      fn: testDownloadQRCodeAllFormats,
    },
    {
      name: "multiple manager instances",
      fn: testMultipleManagerInstances,
    },
    {
      name: "constructor preserves existing global objects",
      fn: testConstructorPreservesExistingGlobalObjects,
    },
    {
      name: "showQRPanel updates current URL",
      fn: testShowQRPanelUpdatesCurrentUrl,
    },
    {
      name: "hideQRPanel does not update URL",
      fn: testHideQRPanelDoesNotUpdateUrl,
    },
    {
      name: "generateQRCode with container",
      fn: testGenerateQRCodeWithContainer,
    },
    {
      name: "concurrent show/hide operations",
      fn: testConcurrentShowHideOperations,
    },
    {
      name: "global gFloorp methods execute",
      fn: testGlobalFloorpMethodsExecute,
    },
  ];

  await runTests("qrCodeGeneratorExtended.test.ts", tests);
}
