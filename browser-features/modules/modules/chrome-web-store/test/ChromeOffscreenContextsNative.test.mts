// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  install,
  waitFor,
} from "./helpers/resource-paths/resource-path-fixture.ts";
import { offscreenFiles } from "./helpers/offscreen-contexts/fixture-files.mjs";
import type {
  OffscreenParent,
  OffscreenResult,
} from "./helpers/offscreen-contexts/types.ts";
async function runCase(kind: string, index: number): Promise<void> {
  const original = "l".repeat(31) + "abc"[index],
    id = original + "@chromium-extension";
  const owned = await install(offscreenFiles(kind, original, true), id);
  let command: Awaited<ReturnType<typeof owned.control>> | undefined;
  try {
    if (kind !== "document") {
      await waitFor(
        () =>
          owned.extension.backgroundContext?.viewType === "background_worker",
        "actual native Worker",
      );
      assertEquals(
        owned.extension.policy.backgroundTypeModule,
        kind === "module",
        "actual declared module policy",
      );
    }
    command = await owned.control("control.html");
    const nonce = String(Services.uuid.generateUUID());
    const result = await command({
      method: "probe",
      nonce,
    }) as unknown as OffscreenResult;
    const { ExtensionParent } = ChromeUtils.importESModule(
      "resource://gre/modules/ExtensionParent.sys.mjs",
    ) as { ExtensionParent: OffscreenParent };
    const contexts = [
      ...ExtensionParent.ParentAPIManager.proxyContexts.values(),
    ].filter((context) =>
      context.extension.id === id && context.viewType === "offscreen"
    );
    assertEquals(
      contexts.length,
      1,
      "exactly one real native offscreen context",
    );
    const nativeContext = contexts[0].toExtensionContext();
    const windowContext = contexts[0].browsingContext?.currentWindowContext;
    assert(
      nativeContext && windowContext,
      "actual native document and principal exist",
    );
    console.log(
      "[OffscreenContexts]",
      JSON.stringify({
        kind,
        result,
        native: {
          ...nativeContext,
          documentURI: windowContext.documentURI.spec,
          principalOrigin: windowContext.documentPrincipal.originNoSuffix,
          principalAddonId: windowContext.documentPrincipal.addonPolicy?.id,
        },
      }),
    );
    assertEquals(result.runtimeId, original, "exact public extension ID");
    assertEquals(result.worker, kind !== "document", "actual caller realm");
    assertEquals(
      result.module,
      kind === "module",
      "real module-only export effect",
    );
    assertEquals(
      result.identity.runtimeId,
      original,
      "real offscreen public ID",
    );
    assertEquals(
      result.identity.nonce,
      nonce,
      "real offscreen correlated response",
    );
    assertEquals(
      windowContext.documentPrincipal.addonPolicy?.id,
      id,
      "actual principal belongs to this native extension",
    );
    assert(
      windowContext.documentURI.spec.startsWith(owned.extension.baseURI.spec),
      "actual native document URI retains own origin",
    );
    assertEquals(
      result.unfiltered.length,
      1,
      "unfiltered public query finds offscreen document",
    );
    assertEquals(
      result.unfiltered[0].documentId,
      nativeContext.documentId,
      "public result identifies the actual native document",
    );
    assertEquals(
      result.unfiltered[0].documentOrigin,
      windowContext.documentPrincipal.originNoSuffix,
      "public origin remains the actual security principal origin",
    );
    assertEquals(
      result.unfiltered[0].documentUrl,
      result.requestedURL,
      "public document URL matches public runtime.getURL",
    );
    assertEquals(
      result.filtered.length,
      1,
      "public own-URL filter finds the same offscreen document",
    );
    for (
      const records of [
        result.filtered,
        result.byId,
        result.byDocumentId,
        result.byReturnedOrigin,
      ]
    ) {
      assertEquals(
        records.length,
        1,
        "public filters retain one owned context",
      );
      assertEquals(
        records[0].contextId,
        result.unfiltered[0].contextId,
        "stable actual public context identity",
      );
    }
    assertEquals(
      result.foreign.length,
      0,
      "foreign URL filter cannot return own context",
    );
    const cleanup = await command({
      method: "cleanup",
    }) as unknown as OffscreenResult;
    console.log(
      "[OffscreenContextsCleanup]",
      JSON.stringify({ kind, cleanup }),
    );
    assertEquals(
      cleanup.remaining?.length,
      0,
      "real offscreen closure removes public context",
    );
    command = undefined;
  } finally {
    try {
      await command?.({ method: "cleanup" });
    } finally {
      await owned.cleanup();
    }
  }
}
export async function runAllTests(): Promise<void> {
  await runTests(
    "ChromeOffscreenContextsNative",
    ["document", "classic", "module"].map((kind, index) => ({
      name: "public offscreen context URL and identity: " + kind,
      fn: () => runCase(kind, index),
    })),
  );
}
