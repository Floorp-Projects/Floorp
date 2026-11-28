import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "node:path";
import fs from "node:fs";
import { fileURLToPath } from "node:url";
import { genJarmnPlugin } from "../../libs/vite-plugin-gen-jarmn/plugin.ts";

const r = (dir: string) =>
  path.resolve(path.dirname(fileURLToPath(import.meta.url)), dir);

/**
 * Vite plugin to include public directory files in the JAR manifest.
 *
 * Unlike other pages that use index.html as the Vite entry point,
 * pages-aboutDialog uses a separate HTML file in public/ because it
 * contains chrome:// protocol URLs that Vite cannot resolve.
 * This plugin ensures those public files are visible to genJarmnPlugin
 * so they get listed in jar.mn for production packaging.
 */
function includePublicInJarmn() {
  let publicDir = "";
  return {
    name: "include-public-in-jarmn",
    configResolved(config: { publicDir: string }) {
      publicDir = config.publicDir;
    },
    generateBundle(
      _options: unknown,
      bundle: Record<string, { fileName: string }>,
    ) {
      if (!publicDir || !fs.existsSync(publicDir)) return;
      for (const entry of fs.readdirSync(publicDir)) {
        const filePath = path.join(publicDir, entry);
        if (!fs.statSync(filePath).isFile()) continue;
        const alreadyInBundle = Object.values(bundle).some(
          (b) => b.fileName === entry,
        );
        if (alreadyInBundle) continue;
        // @ts-expect-error -- emitFile is available in generateBundle context
        this.emitFile({
          type: "asset",
          fileName: entry,
          source: fs.readFileSync(filePath),
        });
      }
    },
  };
}

export default defineConfig({
  cacheDir: "../../node_modules/.vite/pages-aboutdialog",
  plugins: [
    react({
      jsxRuntime: "automatic",
      jsxImportSource: "preact",
    }),
    // deno-lint-ignore no-explicit-any
    includePublicInJarmn() as any,
    genJarmnPlugin(
      "pages-aboutdialog",
      "noraneko-pages-aboutdialog",
      "content",
      {
        overrides: [
          "chrome://browser/content/aboutDialog.xhtml chrome://noraneko-pages-aboutdialog/content/aboutDialog.html",
        ],
      },
      // deno-lint-ignore no-explicit-any
    ) as any,
  ],
  resolve: {
    alias: {
      react: "preact/compat",
      "react-dom": "preact/compat",
    },
  },
  build: {
    outDir: "_dist",
    emptyOutDir: true,
    target: "esnext",
    rollupOptions: {
      input: {
        main: r("src/main.tsx"),
      },
      output: {
        entryFileNames: "[name].js",
        format: "iife",
      },
    },
  },
});
