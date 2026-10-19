/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @colocated-env browser

import { installDocumentIdPolyfill } from "../../../polyfills/documentId/DocumentIdPolyfill.sys.mts";
import { DOCUMENT_ID_POLYFILL_SOURCE } from "../../../polyfills/documentId/DocumentIdPolyfillSource.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../../../chrome/test/utils/test_harness.ts";

function testInstallerPreservesNativeAPIs(): void {
  const globals = globalThis as unknown as Record<string, unknown>;
  const chromeBefore = Object.getOwnPropertyDescriptor(globals, "chrome");
  const browserBefore = Object.getOwnPropertyDescriptor(globals, "browser");

  assertEquals(
    installDocumentIdPolyfill({ force: true }),
    false,
    "Synthetic IDs must remain disabled even when the old force option is set",
  );
  assertEquals(
    Object.getOwnPropertyDescriptor(globals, "chrome")?.value,
    chromeBefore?.value,
    "The native chrome namespace must retain its identity",
  );
  assertEquals(
    Object.getOwnPropertyDescriptor(globals, "browser")?.value,
    browserBefore?.value,
    "The native browser namespace must retain its identity",
  );
}

function testLegacySourcePreservesNativeBindings(): void {
  const sandbox = Cu.Sandbox(
    Services.scriptSecurityManager.getSystemPrincipal(),
    {
      freshCompartment: true,
    },
  );
  try {
    Cu.evalInSandbox(
      `
      const scripting = Object.freeze({ executeScript() {} });
      const tabs = Object.freeze({ sendMessage() {}, connect() {} });
      const nativeAPI = Object.freeze({ scripting, tabs });
      Object.defineProperty(this, "chrome", { value: nativeAPI });
      Object.defineProperty(this, "browser", { value: nativeAPI });
      Object.defineProperty(this, "window", { value: this });
    `,
      sandbox,
    );
    Cu.evalInSandbox(DOCUMENT_ID_POLYFILL_SOURCE, sandbox);
    assertEquals(
      Cu.evalInSandbox(
        "chrome === nativeAPI && browser === nativeAPI && " +
          "chrome.scripting === scripting && chrome.tabs === tabs && " +
          '!("__floorpPolyfills" in chrome)',
        sandbox,
      ),
      true,
      "The legacy script must leave native methods and namespaces intact",
    );
  } finally {
    Cu.nukeSandbox(sandbox);
  }
}

interface DocumentWindow {
  innerWindowId: number;
  documentURI: { spec: string };
}

interface TestTab {
  linkedBrowser: {
    browsingContext: { currentWindowGlobal: DocumentWindow | null };
    loadURI(uri: unknown, options: { triggeringPrincipal: unknown }): void;
  };
}

async function testNativeIdsFollowDocumentLifetime(): Promise<void> {
  const { ExtensionDocumentId } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionDocumentId.sys.mjs",
  );
  const { setTimeout: delay } = ChromeUtils.importESModule(
    "resource://gre/modules/Timer.sys.mjs",
  );
  const browserWindow = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as {
    gBrowser: {
      addTab(url: string, options: { triggeringPrincipal: unknown }): TestTab;
      removeTab(tab: TestTab): void;
    };
  };
  assert(browserWindow?.gBrowser, "A browser window is required");
  const principal = Services.scriptSecurityManager.getSystemPrincipal();
  const firstURL = "data:text/html,<title>document-id-first</title>";
  const secondURL = "data:text/html,<title>document-id-second</title>";
  const tab = browserWindow.gBrowser.addTab(firstURL, {
    triggeringPrincipal: principal,
  });
  const browser = tab.linkedBrowser;

  async function waitForDocument(url: string): Promise<DocumentWindow> {
    const deadline = Date.now() + 10000;
    while (Date.now() < deadline) {
      const windowGlobal = browser.browsingContext.currentWindowGlobal;
      if (windowGlobal?.documentURI.spec === url) {
        return windowGlobal;
      }
      await new Promise<void>((resolve) => delay(resolve, 20));
    }
    throw new Error(`Timed out waiting for ${url}`);
  }

  try {
    const firstWindow = await waitForDocument(firstURL);
    const firstId = ExtensionDocumentId.getDocumentId(
      firstWindow.innerWindowId,
    );
    assert(
      typeof firstId === "string" &&
        /^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$/i.test(firstId),
      "The runtime must issue an opaque document UUID",
    );
    assertEquals(
      ExtensionDocumentId.getDocumentId(firstWindow.innerWindowId),
      firstId,
      "One document must have a stable ID across API consumers",
    );
    assertEquals(
      ExtensionDocumentId.getBrowsingContextForDocumentId(firstId),
      browser.browsingContext,
      "The native ID must resolve to its current browsing context",
    );

    browser.loadURI(Services.io.newURI(secondURL), {
      triggeringPrincipal: principal,
    });
    const secondWindow = await waitForDocument(secondURL);
    const secondId = ExtensionDocumentId.getDocumentId(
      secondWindow.innerWindowId,
    );
    assert(firstId !== secondId, "Navigation must replace the document ID");
    assertEquals(
      ExtensionDocumentId.getBrowsingContextForDocumentId(firstId),
      null,
      "A stale ID must not resolve to the new document in the same frame",
    );
    assertEquals(
      ExtensionDocumentId.getBrowsingContextForDocumentId(secondId),
      browser.browsingContext,
      "The new document must resolve in the original browsing context",
    );
    assertEquals(
      ExtensionDocumentId.getBrowsingContextForDocumentId("floorp-doc-1-0-old"),
      null,
      "A synthetic legacy ID must never select a live frame",
    );
  } finally {
    browserWindow.gBrowser.removeTab(tab);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("DocumentIdPolyfill.test.mts", [
    {
      name: "Native APIs survive legacy installation",
      fn: testInstallerPreservesNativeAPIs,
    },
    {
      name: "Native bindings survive a legacy generated script",
      fn: testLegacySourcePreservesNativeBindings,
    },
    {
      name: "Native document IDs expire on navigation",
      fn: testNativeIdsFollowDocumentLifetime,
    },
  ]);
}
