import path from "node:path";
import { defineConfig } from "vite";

const r = (subpath: string): string =>
  path.resolve(import.meta.dirname, subpath);

export default defineConfig({
  build: {
    outDir: r("_dist"),
    target: "firefox128",
    lib: {
      formats: ["es"],
      entry: [r("src/chrome_root.ts"), r("src/about-preferences.ts")],
    },
    rollupOptions: {
      external(source, importer, isResolved) {
        if (source.startsWith("chrome://")) {
          return true;
        }
        return false;
      },
    },
  },
});
