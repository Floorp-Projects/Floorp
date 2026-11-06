import { defineConfig } from "vite";
import react from "@vitejs/plugin-react-swc";
import { fileURLToPath } from "node:url";

export default defineConfig({
  root: fileURLToPath(new URL(".", import.meta.url)),
  base: "./",
  plugins: [react()],
  server: {
    fs: { allow: [fileURLToPath(new URL("../../..", import.meta.url))] },
  },
  build: { assetsInlineLimit: 200000, target: "es2022" },
});
