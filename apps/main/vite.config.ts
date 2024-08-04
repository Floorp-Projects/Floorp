import { defineConfig } from "vite";
import path from "node:path";
import tsconfigPaths from "vite-tsconfig-paths";
import solidPlugin from "vite-plugin-solid";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

export default defineConfig({
  publicDir: r("public"),
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    modulePreload: false,
    target: "firefox128",

    rollupOptions: {
      //https://github.com/vitejs/vite/discussions/14454
      preserveEntrySignatures: "allow-extension",
      input: {
        core: "./core/index.ts",
        "about-preferences": "./about/preferences/index.ts",
        env: "./experiment/env.ts",
      },
      output: {
        esModule: true,
        entryFileNames: "content/[name].js",
      },
    },

    outDir: r("_dist"),
    assetsDir: "content/assets",
  },

  //? https://github.com/parcel-bundler/lightningcss/issues/685
  //? lepton uses System Color and that occurs panic.
  //? when the issue resolved, gladly we can use lightningcss
  // css: {
  //   transformer: "lightningcss",
  // },

  plugins: [
    tsconfigPaths(),
    {
      name: "solid-xul-refresh",
      apply: "serve",
      handleHotUpdate(ctx) {
        console.log(`handle hot : ${JSON.stringify(ctx.modules)}`);
      },
    },
    solidPlugin({
      solid: {
        generate: "universal",
        moduleName: "@nora/solid-xul",
        contextToCustomElements: false,
      },
      hot: false,
    }),
  ],
  optimizeDeps: {
    include: ["./node_modules/@nora"],
  },
  resolve: {
    preserveSymlinks: true,
  },
});
