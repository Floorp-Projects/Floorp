import { defineConfig, mergeConfig } from "vite";
import base from "../../vite.config.ts";

export default defineConfig(async (env) =>
  mergeConfig(await (typeof base === "function" ? base(env) : base), {
    cacheDir: "../../node_modules/.vite/settings-tests",
    server: { port: 5196, strictPort: true, hmr: false },
  })
);
