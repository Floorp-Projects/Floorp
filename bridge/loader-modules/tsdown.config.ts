import { defineConfig } from "tsdown";
import { genJarmnPlugin } from "@nora/vite-plugin-gen-jarmn";

export default defineConfig({
  entry: ["link-modules/**/*.mts"],
  outDir: "_dist",
  format: "esm",
  target: "esnext",
  external: /^resource:\/\/|^chrome:\/\//,
  plugins: [genJarmnPlugin("resource", "noraneko-resource", "resource")],
});
