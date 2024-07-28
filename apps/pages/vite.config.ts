import { defineConfig } from "vite";
import path from "node:path";
import tsconfigPaths from "vite-tsconfig-paths";
import solidPlugin from "vite-plugin-solid";
import Icons from "unplugin-icons/vite";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

export default defineConfig({
  appType: "mpa",
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    target: "firefox128",

    outDir: r("_dist"),
    rollupOptions: {
      input: {
        "nora-settings": "./nora-settings/index.html",
      },
    },
  },

  css: {
    transformer: "lightningcss",
  },

  plugins: [Icons({ compiler: "solid" }), solidPlugin()],
});
