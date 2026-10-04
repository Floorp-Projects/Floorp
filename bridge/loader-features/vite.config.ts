// SPDX-License-Identifier: MPL-2.0

import { defineConfig } from "vite";
import { realpathSync } from "node:fs";
import path from "node:path";

import istanbulPlugin from "vite-plugin-istanbul";
import swc from "unplugin-swc";
import { genJarmnPlugin } from "../../libs/vite-plugin-gen-jarmn/plugin.ts";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

const dndKitCoreDir = realpathSync(
  r("../../browser-features/pages-settings/node_modules/@dnd-kit/core"),
);
const dndKitAccessibilityDir = realpathSync(
  path.join(path.dirname(dndKitCoreDir), "accessibility"),
);
const REACT_UI_PATH =
  /[\\/](?:browser-features[\\/]pages-settings|libs[\\/]ui)[\\/]/;
// An explicit SWC include replaces its default source-extension filter. Keep
// CSS modules and other assets in Vite's own transform pipeline.
const REACT_UI_SOURCE_PATH =
  /[\\/](?:browser-features[\\/]pages-settings|libs[\\/]ui)[\\/].*\.[cm]?[jt]sx?(?:\?.*)?$/;
const chakraReactDir = realpathSync(
  r("../../libs/ui/node_modules/@chakra-ui/react"),
);
const emotionReactDir = realpathSync(
  r("../../libs/ui/node_modules/@emotion/react"),
);
const reactRouterDomDir = realpathSync(
  r("../../browser-features/pages-settings/node_modules/react-router-dom"),
);

export default defineConfig({
  cacheDir: "../../node_modules/.vite/loader-features",
  publicDir: r("public"),

  server: {
    port: 5181,
    strictPort: true,
    cors: {
      origin: [
        /^https?:\/\/(localhost|127\.0\.0\.1)(?::\d+)?$/,
        /^moz-extension:\/\/.+$/,
        /^chrome:\/\/.+$/,
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
      exclude: [/node_modules/, "**/*.tsx", REACT_UI_PATH],
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

    // unplugin-swc disables Vite's esbuild transform; compile chrome JSX here
    // while keeping the shared settings UI on its separate React transform.
    swc.vite({
      include: /\.[jt]sx$/,
      exclude: [/node_modules/, REACT_UI_PATH],
      jsc: {
        target: "esnext",
        parser: { syntax: "typescript", tsx: true, decorators: true },
        transform: {
          decoratorVersion: "2022-03",
          react: {
            runtime: "automatic",
            importSource: "preact",
            throwIfNamespace: false,
            development: false,
            refresh: false,
          },
        },
      },
    }),

    // Hub tests and shared UI are loaded directly into a privileged document instead
    // of through Vite's HTML pipeline. Compile their React JSX without Fast
    // Refresh so the output does not depend on the injected refresh preamble.
    swc.vite({
      include: REACT_UI_SOURCE_PATH,
      jsc: {
        target: "esnext",
        parser: {
          syntax: "typescript",
          tsx: true,
          decorators: true,
        },
        transform: {
          react: {
            runtime: "automatic",
            development: false,
            refresh: false,
          },
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

    istanbulPlugin(),
    genJarmnPlugin("content", "noraneko", "content"),
  ],

  // 既存の設定...
  optimizeDeps: {
    ignoreOutdatedRequests: true,
    noDiscovery: true,
    esbuildOptions: {
      // The workspace uses isolated symlinks. Following them lets the browser
      // test bundle resolve each package's transitive dependencies from its
      // real Deno npm cache location.
      preserveSymlinks: false,
    },
    include: [
      // Page tests import React helpers (for example the settings search index).
      // With discovery disabled, CJS React must be explicitly converted to ESM.
      "react",
      "react/jsx-runtime",
      "./node_modules/@nora",
      // Shared settings controls depend on Chakra and Emotion. Discovery is
      // disabled, so prebundle their CJS transitive dependencies explicitly.
      "@chakra-ui/react",
      "@emotion/react",
      "@dnd-kit/core",
      "@dnd-kit/modifiers",
      "@dnd-kit/sortable",
      "@dnd-kit/utilities",
      "clsx",
      "i18next",
      "lucide-react",
      "react",
      "react-dom",
      "react-dom/client",
      "react-i18next",
      "react-router-dom",
      "react/jsx-runtime",
      "tailwind-merge",
      "preact",
      "preact/hooks",
      "preact/compat",
      "@preact/signals",
      "@preact/signals-core",
    ],
  },

  resolve: {
    dedupe: [
      "react",
      "react-dom",
      "preact",
      "preact/hooks",
      "preact/compat",
      "@preact/signals",
      "@preact/signals-core",
    ],
    preserveSymlinks: true,
    alias: [
      { find: "@chakra-ui/react", replacement: chakraReactDir },
      { find: "@emotion/react", replacement: emotionReactDir },
      { find: "react-router-dom", replacement: reactRouterDomDir },
      {
        find: "@dnd-kit/accessibility",
        replacement: dndKitAccessibilityDir,
      },
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
      {
        find: "#firefox-tests",
        replacement: r("../../_dist/firefox-tests/files"),
      },
      {
        find: "@",
        replacement: r("../../browser-features/pages-settings/src"),
      },
      {
        find: "#libs",
        replacement: r("../../libs"),
      },
    ],
  },
});
