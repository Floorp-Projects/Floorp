import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "@nora/vite-plugin-gen-jarmn";

export default defineConfig({
  build: {
    outDir: "_dist",
  },
  plugins: [
    tailwindcss(),
    react({
      jsxImportSource: "react",
    }),
    tsconfigPaths(),
    genJarmnPlugin("content-newtab", "noraneko-newtab", "content"),
  ],
  optimizeDeps: {
    include: ["react", "react-dom", "react/jsx-runtime"],
  },
  server: {
    hmr: {
      overlay: true,
    },
  },
});
