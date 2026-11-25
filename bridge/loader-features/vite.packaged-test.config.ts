// SPDX-License-Identifier: MPL-2.0
// Explicit QA build: never imported by the production Vite configuration.
import { defineConfig, normalizePath } from "vite";
import { createHash } from "node:crypto";
import { readFileSync, realpathSync } from "node:fs";
import path from "node:path";

const directory = import.meta.dirname;
if (!directory) {
  throw new Error("Packaged QA configuration must be a local file.");
}
const root = path.resolve(directory, "../..");
const requested: unknown = JSON.parse(
  Deno.env.get("FLOORP_QA_TEST_MODULES") ?? "null",
);
if (!Array.isArray(requested) || requested.length === 0) {
  throw new Error(
    "FLOORP_QA_TEST_MODULES must be a nonempty JSON array of repository paths.",
  );
}
const modules = requested.map((file: unknown) => {
  if (
    typeof file !== "string" || file.includes("\\") ||
    file.split("/").some((part) => part === ".." || part === "." || !part) ||
    !/^browser-features\/(chrome|modules)\/.+\.test\.(m?ts|tsx|m?js|jsx)$/.test(
      file,
    )
  ) throw new Error(`Unsupported packaged test path: ${String(file)}`);
  const absolute = realpathSync(path.resolve(root, file));
  const relative = path.relative(realpathSync(root), absolute);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new Error(`Packaged test escapes checkout: ${file}`);
  }
  const bytes = readFileSync(absolute);
  if (!bytes.toString("utf8").includes("@colocated-env browser")) {
    throw new Error(`Missing browser test marker: ${file}`);
  }
  return {
    file,
    absolute: normalizePath(absolute),
    sha256: createHash("sha256").update(bytes).digest("hex"),
  };
});
if (new Set(modules.map((entry) => entry.file)).size !== modules.length) {
  throw new Error("Duplicate packaged test module.");
}
const discovery = normalizePath(
  path.resolve(directory, "loader/test/discovery.ts"),
);

export default defineConfig({
  base: "chrome://noraneko/content/qa/",
  publicDir: false,
  build: {
    outDir: "_dist/qa",
    emptyOutDir: true,
    target: "firefox133",
    minify: false,
    modulePreload: false,
    rollupOptions: {
      input: path.resolve(directory, "loader/test/packaged.ts"),
      preserveEntrySignatures: "strict",
      external: /^(resource|chrome):\/\//,
      output: {
        entryFileNames: "packaged-tests.js",
        chunkFileNames: "chunks/[name]-[hash].js",
      },
    },
  },
  plugins: [{
    name: "packaged-qa-scoped-discovery",
    enforce: "pre",
    load(id) {
      if (normalizePath(id) !== discovery) return;
      return `export async function discoverTestModules() { return {${
        modules.map(({ file, absolute }) =>
          `${JSON.stringify(file)}: () => import(${JSON.stringify(absolute)})`
        ).join(",")
      }}; }`;
    },
    generateBundle() {
      this.emitFile({
        type: "asset",
        fileName: "qa-test-manifest.json",
        source: JSON.stringify(
          {
            schemaVersion: 1,
            modules: modules.map(({ file, sha256 }) => ({ file, sha256 })),
          },
          null,
          2,
        ),
      });
    },
  }],
});
