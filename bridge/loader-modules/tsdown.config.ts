import { defineConfig } from "tsdown";

export default defineConfig({
  entry: ["link-modules/**/*.mts"],
  outDir: "_dist",
  format: "esm",
  target: "esnext",
  external: /^resource:\/\/|^chrome:\/\//,
});
