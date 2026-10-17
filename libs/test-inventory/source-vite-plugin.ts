// SPDX-License-Identifier: MPL-2.0

import path from "node:path";
import { realpath } from "node:fs/promises";
import type { Plugin } from "vite";
import { browserDiscoverable } from "./contract.ts";
import { readSourceBuffer } from "./source-files.ts";
import {
  parseTestSourceManifest,
  TEST_SOURCE_DRIVER,
  TEST_SOURCE_EXPORT,
  testSourceManifestFilename,
} from "./source-manifest.ts";
import type { TestSourceContext, TestSourceManifest } from "./source-types.ts";

const DRIVER_ID = "bridge/loader-features/loader/test/index.ts";

/** Binds entry-source compile inputs, not transforms, imports or final products. */
export function testSourceBindingPlugin(projectRoot: string): Plugin {
  let context: TestSourceContext | undefined;
  let manifest: TestSourceManifest | undefined;
  let bootstrapOnly = false;
  let driverBinding: Promise<void> | undefined;
  const delivered = new Map<string, string>();

  async function bindDriver(physicalRoot: string): Promise<void> {
    if (context || bootstrapOnly) return;
    const profile = path.join(physicalRoot, "_dist/profile/test");
    const controlPath = path.join(profile, "nora-tests-control.json");
    if (await realpath(controlPath) !== controlPath) {
      throw new Error("Source control path is redirected");
    }
    const control = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(
      (await readSourceBuffer(controlPath)).bytes,
    )) as Record<string, unknown>;
    if (
      control.purpose === "restart-bootstrap-isolation" &&
      control.sourceManifestSha256 === undefined
    ) {
      // Only the existing browser one-shot/lease checks can grant NOT_EXECUTED.
      // This server can never be upgraded to an ordinary source-bound run.
      bootstrapOnly = true;
      return;
    }
    if (
      typeof control.runId !== "string" ||
      typeof control.sourceManifestSha256 !== "string" ||
      !/^[a-f0-9]{64}$/.test(control.sourceManifestSha256)
    ) {
      throw new Error("Original source-bound browser control is required");
    }
    const filename = path.join(
      profile,
      testSourceManifestFilename(control.runId),
    );
    if (await realpath(filename) !== filename) {
      throw new Error("Source manifest path is redirected");
    }
    const source = await readSourceBuffer(filename);
    if (source.sha256 !== control.sourceManifestSha256) {
      throw new Error("Original source manifest digest differs");
    }
    const parsed = parseTestSourceManifest(JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(source.bytes),
    ));
    if (
      parsed.runId !== control.runId ||
      parsed.expiresAtMs !== control.expiresAtMs
    ) {
      throw new Error("Source manifest/control run or expiry differs");
    }
    manifest = parsed;
    context = {
      schemaVersion: 1,
      runId: parsed.runId,
      manifestSha256: source.sha256,
    };
  }

  function requireLiveManifest(): TestSourceManifest {
    if (!context || !manifest || Date.now() >= manifest.expiresAtMs) {
      throw new Error(
        "Missing or expired bound source run; start a fresh server",
      );
    }
    return manifest;
  }

  return {
    name: "floorp-test-source-binding",
    enforce: "pre",
    apply: (_config, env) => env.command === "serve" && env.mode === "test",
    config: () => ({ server: { hmr: false } }),
    async load(rawId) {
      if (rawId.startsWith("\0")) return null;
      const parts = rawId.split("?");
      const filename = parts[0];
      const relative = path.relative(projectRoot, filename).replaceAll(
        "\\",
        "/",
      );
      const candidate = relative === DRIVER_ID ||
        filename.replaceAll("\\", "/").endsWith("/" + DRIVER_ID) ||
        browserDiscoverable(relative) ||
        /(?:^|\/)(?:browser-features\/(?:chrome|modules|pages-[^/]+)|link-features-chrome)\/.*\.test\.[cm]?[jt]sx?$/
          .test(filename.replaceAll("\\", "/"));
      if (!candidate) return null;
      if (parts.length > 2 || (parts[1] && !/^t=\d+$/.test(parts[1]))) {
        throw new Error("Unsupported query on a bound test source");
      }
      const alias = "bridge/loader-features/link-features-chrome/";
      const logicalId = relative.startsWith(alias)
        ? "browser-features/chrome/" + relative.slice(alias.length)
        : relative;
      if (logicalId !== DRIVER_ID && !browserDiscoverable(logicalId)) {
        throw new Error(
          "Test source is outside the exact logical project tree",
        );
      }
      const physicalRoot = await realpath(projectRoot);
      const physicalFile = await realpath(filename);
      if (physicalFile !== path.join(physicalRoot, ...logicalId.split("/"))) {
        throw new Error(
          "Test source logical path is redirected to another source",
        );
      }
      const id = path.relative(physicalRoot, physicalFile).replaceAll(
        "\\",
        "/",
      );
      if (id !== logicalId || (!browserDiscoverable(id) && id !== DRIVER_ID)) {
        throw new Error(
          "Test source resolves outside the exact project source tree",
        );
      }
      if (id === DRIVER_ID) {
        driverBinding ??= bindDriver(physicalRoot);
        await driverBinding;
      }
      if (bootstrapOnly && id !== DRIVER_ID) {
        throw new Error(
          "Restart bootstrap server cannot deliver QA test sources",
        );
      }
      if (!bootstrapOnly) requireLiveManifest();
      const source = await readSourceBuffer(physicalFile);
      const code = new TextDecoder("utf-8", { fatal: true }).decode(
        source.bytes,
      );
      if (delivered.has(id) && delivered.get(id) !== source.sha256) {
        throw new Error("Source changed after this server first delivered it");
      }
      if (id === DRIVER_ID) {
        if (!bootstrapOnly) requireLiveManifest();
        delivered.set(id, source.sha256);
        return {
          code: bootstrapOnly ? code : code +
            "\nimport.meta." + TEST_SOURCE_DRIVER + " = Object.freeze(" +
            JSON.stringify(context) + ");\n",
          map: null,
        };
      }
      const current = requireLiveManifest();
      const expected = current.sources.find((entry) => entry.id === id);
      if (!expected || expected.sha256 !== source.sha256) {
        throw new Error("Source digest differs from original run plan: " + id);
      }
      if (code.includes(TEST_SOURCE_EXPORT)) {
        throw new Error("Source uses the reserved source-proof export");
      }
      delivered.set(id, source.sha256);
      const proof = { ...context, id, sourceSha256: source.sha256 };
      // Recheck after I/O; a lease expiring during a read cannot serve code.
      requireLiveManifest();
      return {
        code: code + "\nexport const " + TEST_SOURCE_EXPORT +
          " = Object.freeze(" + JSON.stringify(proof) + ");\n",
        map: null,
      };
    },
  };
}
