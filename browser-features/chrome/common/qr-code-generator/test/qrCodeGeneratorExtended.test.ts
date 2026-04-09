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
  const expectedUrl =
    globalThis.gBrowser?.selectedTab?.linkedBrowser?.currentURI?.spec ?? "";

  manager.updateCurrentTabUrl();
  assertEquals(
    manager.currentUrl(),
    expectedUrl,
    "currentUrl should reflect selected tab URL",
  );
}

function testDownloadQRCodeDelegatesToUnderlyingInstance(): void {
  const manager = new QRCodeManager() as QRCodeManager & {
    qrCode?: {
      download: (args: { name: string; extension: string }) => void;
    };
  };

  let called = false;
  let extension = "";
  manager.qrCode = {
    download: (args) => {
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
  ];

  await runTests("qrCodeGeneratorExtended.test.ts", tests);
}
