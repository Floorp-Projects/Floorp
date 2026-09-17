import { defineConfig, mergeConfig } from "vite";
import base from "../../vite.config.ts";
export default defineConfig(async (env) =>
  mergeConfig(await (typeof base === "function" ? base(env) : base), {
    cacheDir: "../../node_modules/.vite/welcome-tests",
    server: { port: 5197, strictPort: true, hmr: false },
  })
);
