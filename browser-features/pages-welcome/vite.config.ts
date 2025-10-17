import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "@nora/vite-plugin-gen-jarmn";
import { disableCspInDevPlugin } from "@nora/vite-plugin-disable-csp";

export default defineConfig(({ command }) => ({
  build: {
    outDir: "_dist",
  },
  plugins: [
    includeIndexHtmlPlugin({ isIndexOwner: true }),
    tailwindcss(),
    react({
      jsxImportSource: "react",
    }),
    tsconfigPaths(),
    genJarmnPlugin("content-welcome", "noraneko-welcome", "content"),
    disableCspInDevPlugin(command === "serve"),
  ],
  optimizeDeps: {
    include: ["react", "react-dom", "react/jsx-runtime"],
  },
  server: {
    hmr: {
      overlay: true,
    },
  },
}));
