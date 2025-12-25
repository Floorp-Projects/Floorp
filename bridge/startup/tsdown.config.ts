// SPDX-License-Identifier: MPL-2.0

import { defineConfig } from "tsdown/config";
import { genJarmnPlugin } from "#libs/vite-plugin-gen-jarmn/plugin.ts";

export default [
  defineConfig({
    entry: [
      "src/chrome_root.ts",
      "src/about-preferences.ts",
      "src/about-newtab.ts",
    ],
    outDir: "_dist",
    platform: "browser",
    treeshake: false,
    plugins: [genJarmnPlugin("startup", "noraneko-startup", "content")],
    external: /^resource:\/\/|^chrome:\/\//g,
  }),
];
