import { fontLicensesPlugin } from "../../libs/ui/vite-font-licenses.ts";
import process from "node:process";
import { defineConfig, searchForWorkspaceRoot } from "vite";
import { fileURLToPath } from "node:url";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "../../libs/vite-plugin-gen-jarmn/plugin.ts";
import { disableCspInDevPlugin } from "../../libs/vite-plugin-disable-csp/plugin.ts";

export default defineConfig(({ command }) => {
  if (command === "serve") process.env.NODE_ENV = "development";
  return {
    cacheDir: "../../node_modules/.vite/pages-settings",
    build: {
      assetsInlineLimit: 0,
      outDir: "_dist",
    },
    resolve: {
      dedupe: ["react", "react-dom"],
    },
    plugins: [
      tailwindcss(),
      react({
        jsxImportSource: "react",
      }),
      tsconfigPaths(),
      fontLicensesPlugin(),
      genJarmnPlugin("content-settings", "noraneko-settings", "content"),
      disableCspInDevPlugin(command === "serve"),
    ],
    optimizeDeps: {
      include: ["react", "react-dom", "react/jsx-runtime"],
    },
    server: {
      fs: {
        allow: [
          searchForWorkspaceRoot(process.cwd()),
          fileURLToPath(new URL("../../libs/ui", import.meta.url)),
        ],
      },
      watch: {
        // Vite removes bundled configs after loading. Watching those transient
        // files can terminate Deno's watcher when another build runs.
        ignored: ["**/vite.config.*.timestamp-*.mjs"],
      },
      hmr: {
        overlay: true,
      },
    },
  };
});
