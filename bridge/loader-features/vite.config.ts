// vite.config.ts (修正版)
import { defineConfig } from "rolldown-vite";
import path from "node:path";
import solidPlugin from "vite-plugin-solid";
import istanbulPlugin from "vite-plugin-istanbul";
import deno from "@deno/vite-plugin";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

export default defineConfig({
  publicDir: r("public"),
  server: {
    port: 5181,
    strictPort: true,
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
        "about-preferences": r(
          "../../../../src/ui/about-pages/preferences/index.ts",
        ),
        "about-newtab": r("../../../../src/ui/about-pages/newtab/index.ts"),
        //env: "./experiment/env.ts",
      },
      output: {
        esModule: true,
        entryFileNames: "[name].js",
        manualChunks(id, meta) {
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
          } catch {}
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
        chunkFileNames(chunkInfo) {
          return "assets/js/[name].js";
        },
      },
    },

    outDir: r("_dist"),
  },

  plugins: [
    deno(),
    
    // 既存のプラグイン...
    solidPlugin({
      solid: {
        generate: "universal",
        moduleName: "@nora/solid-xul",
        contextToCustomElements: false,
        hydratable: true,
      },
      hot: false,
    }),
  
    
    // HMR支援プラグイン
    {
      name: "noraneko_component_hmr_support",
      enforce: "pre",
      apply: "serve",
      transform(code, id, options) {
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
    
    istanbulPlugin({}),
  ],
  
  // 既存の設定...
  optimizeDeps: {
    include: [
      "./node_modules/@nora",
      "solid-js",
      "solid-js/web",
      "solid-js/store",
      "solid-js/html",
      "solid-js/h",
    ],
  },
  
  resolve: {
    dedupe: [
      "solid-js",
      "solid-js/web", 
      "solid-js/store",
      "solid-js/html",
      "solid-js/h",
    ],
    preserveSymlinks: true,
    alias: [
      { find: "@nora/skin", replacement: r("../../../../src/themes") },
      {
        find: "../../../../../shared",
        replacement: r("../../../../src/shared"),
      },
      { find: "#apps", replacement: r("../../../../apps") },
      {
        find: "#i18n-features-chrome",
        replacement: r("./link-i18n-features-chrome"),
      },
      {
        find: "#features-chrome",
        replacement: r("./link-features-chrome"),
      },
    ],
  },
})