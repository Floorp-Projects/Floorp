// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  checkCompatibility,
  transformManifestFull,
} from "../ManifestTransformer.sys.mts";
import type { ChromeManifest } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

export async function runAllTests(): Promise<void> {
  await runTests("NativeBackgroundSelection.test.mts", [
    {
      name: "native capability preserves classic and module worker manifests",
      fn() {
        for (const type of [undefined, "classic", "module"] as const) {
          const manifest: ChromeManifest = {
            manifest_version: 3,
            name: "Native worker",
            version: "1",
            background: {
              service_worker: "worker.js",
              ...(type ? { type } : {}),
            },
          };
          const transformed = transformManifestFull(manifest, {
            extensionId: "a".repeat(32),
            nativeWorker: true,
          });
          assertEquals(
            JSON.stringify(transformed.manifest.background),
            JSON.stringify(manifest.background),
            "native worker selection preserves original path and script kind",
          );
          assert(
            !transformed.warnings.some((warning) =>
              warning.includes("fallback") || warning.includes("event page")
            ),
            "native worker manifest must not report an event-page fallback",
          );
          assert(
            !checkCompatibility(manifest, { nativeWorker: true }).some(
              (issue) => issue.includes("fallback"),
            ),
            "pure compatibility check respects explicit capability",
          );
          const legacy = transformManifestFull(manifest, {
            extensionId: "a".repeat(32),
          });
          assertEquals(
            legacy.manifest.background?.service_worker,
            undefined,
            "pure default remains the document fallback",
          );
          assert(
            legacy.warnings.some((warning) =>
              warning.includes("service-worker lifecycle")
            ),
            "fallback lifecycle limitation remains explicit",
          );
        }
      },
    },
    {
      name:
        "native compatibility prioritizes workers while preserving cross-browser fields",
      fn() {
        const manifest: ChromeManifest = {
          manifest_version: 3,
          name: "Cross browser",
          version: "1",
          background: {
            service_worker: "worker.js",
            type: "module",
            scripts: ["legacy.js"],
          },
        };
        const result = transformManifestFull(manifest, {
          extensionId: "a".repeat(32),
          nativeWorker: true,
        });
        assertEquals(
          JSON.stringify(result.manifest.background),
          JSON.stringify(manifest.background),
          "native runtime selects compatible worker without rewriting original background fields",
        );
        assertEquals(
          JSON.stringify(manifest.background?.scripts),
          '["legacy.js"]',
          "input manifest remains intact",
        );
      },
    },
  ]);
}
