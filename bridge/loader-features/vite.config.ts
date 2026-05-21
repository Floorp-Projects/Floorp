// SPDX-License-Identifier: MPL-2.0

import { defineConfig } from "vite";
import path from "node:path";

import istanbulPlugin from "vite-plugin-istanbul";
import swc from "unplugin-swc";
import { genJarmnPlugin } from "../../libs/vite-plugin-gen-jarmn/plugin.ts";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};


export default defineConfig({
  publicDir: r("public"),

  oxc: {
    jsx: {
      runtime: "automatic",
      importSource: "preact",
      throwIfNamespace: false,
    },
  },
  server: {
    port: 5181,
    strictPort: true,
    cors: {
      origin: [
        /^https?:\/\/(localhost|127\.0\.0\.1)(?::\d+)?$/,
        /^moz-extension:\/\/.+$/,
        /^chrome:\/\/.+$/,
        // "null" matches requests with `Origin: null` (e.g. file:// pages).
        // Intentional for local development only; do not enable in production.
        "null",
      ],
    },
  },

  define: {
    "import.meta.env.__BUILDID2__": '"placeholder"',
  },

  // 既存のbuild設定...
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    target: "firefox133",

    rollupOptions: {
      //https://github.com/vitejs/vite/discussions/14454
      preserveEntrySignatures: "allow-extension",
      input: {
        core: r("loader/index.ts"),
        // "about-preferences": r(
        //   "../../../../src/ui/about-pages/preferences/index.ts",
        // ),
        // "about-newtab": r("../../../../src/ui/about-pages/newtab/index.ts"),
      },
      output: {
        esModule: true,
        entryFileNames: "[name].js",
        manualChunks(id, _meta) {
          if (id.includes("node_modules")) {
            const arr_module_name = id
              .toString()
              .split("node_modules/")[1]
              .split("/");
            if (arr_module_name[0] === ".pnpm") {
              return `external/${arr_module_name[1].toString()}`;
            }
            return `external/${arr_module_name[0].toString()}`;
          }
          if (id.includes(".svg")) {
            return `svg/${id.split("/").at(-1)?.replaceAll("svg_url", "glue")}`;
          }
          try {
            const re = new RegExp(/\/core\/common\/([A-Za-z-]+)/);
            const result = re.exec(id);
            if (result?.at(1) != null) {
              return `modules/${result[1]}`;
            }
          } catch { /* non-matching module path */ }
        },
        assetFileNames(assetInfo) {
          if (assetInfo.originalFileNames.at(0)?.endsWith(".svg")) {
            return "assets/svg/[name][extname]";
          }
          if (assetInfo.originalFileNames.at(0)?.endsWith(".css")) {
            return "assets/css/[name][extname]";
          }
          return "assets/[name][extname]";
        },
        chunkFileNames(_chunkInfo) {
          return "assets/js/[name].js";
        },
      },
    },

    outDir: r("_dist"),
  },

  plugins: [
    // deno(),

    swc.vite({
      exclude: ["*preact-xul*", "*preact*", "**/*.tsx"],
      jsc: {
        target: "esnext",
        parser: {
          syntax: "typescript",
          decorators: true,
        },
        transform: {
          decoratorMetadata: true,
          decoratorVersion: "2022-03",
        },
      },
    }),

    // HMR支援プラグイン
    {
      name: "noraneko_component_hmr_support",
      enforce: "pre",
      apply: "serve",
      transform(code, _id, _options) {
        if (
          code.includes("\n@noraComponent") &&
          !code.includes("//@nora-only-dispose")
        ) {
          code += "\n";
          code += [
            "if (import.meta.hot) {",
            "  import.meta.hot.accept((m) => {",
            "    if(m && m.default) {",
            "      new m.default();",
            "    }",
            "  })",
            "}",
          ].join("\n");
          return { code };
        }
      },
    },

    ...(process.env.COVERAGE ? [istanbulPlugin()] : []),
    genJarmnPlugin("content", "noraneko", "content"),
  ],

  // 既存の設定...
  optimizeDeps: {
    include: [
      "./node_modules/@nora",
      "preact",
      "preact/hooks",
      "preact/compat",
      "@preact/signals",
      "@preact/signals-core",
    ],
  },

  resolve: {
    dedupe: [
      "preact",
      "preact/hooks",
      "preact/compat",
      "@preact/signals",
      "@preact/signals-core",
    ],
    preserveSymlinks: true,
    alias: [
      { find: "@nora/skin", replacement: r("../../browser-features/skin") },
      {
        find: "@nora/preact-xul/lifetime",
        replacement: r("../../libs/preact-xul/lifetime.ts"),
      },
      {
        find: "@nora/preact-xul",
        replacement: r("../../libs/preact-xul/index.ts"),
      },
      { find: "@std/toml", replacement: "@jsr/std__toml" },
      {
        find: "../../../../../shared",
        replacement: r("../../../../src/shared"),
      },
      { find: "#apps", replacement: r("../../../../apps") },
      {
        find: "#i18n",
        replacement: r("./link-i18n"),
      },
      {
        find: "#features-chrome",
        replacement: r("./link-features-chrome"),
      },
      {
        find: "#features-modules",
        replacement: r("../../browser-features/modules"),
      },
      {
        find: "#features-pages",
        replacement: r("../../browser-features"),
      },
    ],
  },
});
