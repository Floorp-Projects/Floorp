// SPDX-License-Identifier: MPL-2.0

import { defineConfig } from "tsdown";
import { genJarmnPlugin } from "#libs/vite-plugin-gen-jarmn/plugin.ts";

export default defineConfig({
  entry: ["link-modules/**/*.mts"],
  outDir: "_dist",
  format: "esm",
  target: "esnext",
  external: /^resource:\/\/|^chrome:\/\//,
  plugins: [genJarmnPlugin("resource", "noraneko", "resource")],
});
