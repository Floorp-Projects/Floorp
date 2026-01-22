import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "../../libs/vite-plugin-gen-jarmn/plugin.ts";
import { disableCspInDevPlugin } from "../../libs/vite-plugin-disable-csp/plugin.ts";

export default defineConfig(({ command }) => ({
  build: {
    outDir: "_dist",
  },
  plugins: [
    tailwindcss(),
    react({
      jsxImportSource: "react",
    }),
    tsconfigPaths(),
    genJarmnPlugin("content-workflow-progress", "noraneko-workflow-progress", "content"),
    disableCspInDevPlugin(command === "serve"),
  ],
  optimizeDeps: {
    include: ["react", "react-dom", "react/jsx-runtime"],
  },
  server: {
    port: 5192,
    hmr: {
      overlay: true,
    },
  },
}));
