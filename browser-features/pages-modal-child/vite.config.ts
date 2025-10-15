import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "#libs/vite-plugin-gen-jarmn/plugin.ts";

export default defineConfig({
  build: {
    outDir: "_dist",
  },
  plugins: [
    tailwindcss(),
    react(),
    tsconfigPaths(),
    genJarmnPlugin("content-modal-child", "noraneko-modal-child", "content"),
  ],
});
