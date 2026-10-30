// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { resourceFiles } from "./helpers/resource-paths/fixture-files.mjs";
import { install, waitFor } from "./helpers/resource-paths/resource-path-fixture.ts";
async function runCase(
  kind: string,
  compatible: boolean,
  index: number,
): Promise<void> {
  const original = "c".repeat(31) + "defg"[index],
    id = compatible
      ? original + "@chromium-extension"
      : "ordinary-path-" + original + "@example.invalid";
  const owned = await install(
    resourceFiles(kind, compatible, true, original),
    id,
  );
  let command: Awaited<ReturnType<typeof owned.control>> | undefined;
  try {
    if (kind !== "document") {
      await waitFor(
        () =>
          owned.extension.backgroundContext?.viewType === "background_worker",
        "actual nested native Worker",
      );
    }
    if (kind !== "document") {
      assertEquals(
        owned.extension.policy.backgroundTypeModule,
        kind === "module",
        "actual native Worker declared module policy",
      );
    }
    command = await owned.control(
      kind === "document" ? "nested/control.html" : "control.html",
    );
    const registered = await command({ method: "register" });
    assertEquals(
      registered.worker,
      kind !== "document",
      "actual execution realm",
    );
    assertEquals(
      registered.module,
      kind === "module",
      "module-only export executed in actual calling Worker",
    );
    assert(
      registered.location?.endsWith(
        kind === "document" ? "nested/control.html" : "nested/worker.js",
      ),
      "actual nested calling context",
    );
    const prefix = compatible ? "" : "nested/",
      label = compatible ? "root" : "nested";
    assertEquals(
      registered.scripts?.[0].js?.[0],
      prefix + "scripts/registered.js",
      "registered JS path roots correctly",
    );
    assertEquals(
      registered.scripts?.[0].css?.[0],
      prefix + "styles/registered.css",
      "registered CSS path roots correctly",
    );
    const first = await command({
      method: "target",
      url: owned.win.wrappedJSObject.__floorpRuntimeFixtureURL,
    });
    assertEquals(
      first.snapshot?.registered,
      label,
      "registered script actually ran",
    );
    assertEquals(
      first.snapshot?.outline,
      compatible ? "3px" : "7px",
      "registered CSS actually applied",
    );
    const direct = await command({ method: "exercise" });
    assertEquals(
      direct.result?.[0].result,
      label,
      "direct file executes original correct root asset",
    );
    assertEquals(direct.snapshot?.direct, label, "direct file DOM effect");
    assertEquals(
      direct.inserted?.[0].result.border,
      compatible ? "13px" : "19px",
      "insertCSS real computed geometry",
    );
    assertEquals(
      direct.removed?.[0].result,
      direct.before,
      "removeCSS restores prior style",
    );
    const updated = await command({ method: "update" });
    assertEquals(
      updated.snapshot?.registered,
      label + "-updated",
      "updated script on committed new document",
    );
    assertEquals(
      updated.snapshot?.outline,
      compatible ? "4px" : "8px",
      "updated CSS actually applied",
    );
    assertEquals(
      (await command({
        method: "foreign",
        file: "https://example.invalid/not-fetched.js",
      })).rejected,
      true,
      "foreign files rejected",
    );
    const bad = await command({ method: "bad-register" });
    assertEquals(bad.rejected, true, "duplicate existing ID batch rejected");
    assertEquals(bad.scripts?.length, 1, "rejected batch did not register");
    assertEquals(
      bad.scripts?.[0].id,
      "owned-path",
      "original registration remains",
    );
    assertEquals(
      bad.scripts?.[0].js?.[0],
      prefix + "scripts/updated.js",
      "rejected batch preserves original updated asset",
    );
    assertEquals(
      (await command({ method: "cleanup" })).scripts?.length,
      0,
      "actual registrations removed",
    );
  } finally {
    try {
      await command?.({ method: "cleanup" });
    } finally {
      await owned.cleanup();
    }
  }
}
export async function runAllTests(): Promise<void> {
  assertEquals(
    WebExtensionPolicy.backgroundServiceWorkerEnabled,
    false,
    "ordinary global Worker policy unchanged",
  );
  const cases = [["classic", true], ["module", true], ["document", true], [
    "document",
    false,
  ]] as const;
  await runTests(
    "ChromeScriptingPathsNative",
    cases.map(([kind, compatible], index) => ({
      name: `${
        compatible ? "Chrome" : "Firefox"
      } ${kind} native resource path and rejection contract`,
      fn: () => runCase(kind, compatible, index),
    })),
  );
}
