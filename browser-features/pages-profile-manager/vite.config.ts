import { defineConfig } from "vite";
import tailwindcss from "@tailwindcss/vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import { genJarmnPlugin } from "#libs/vite-plugin-gen-jarmn/plugin.ts";
import { disableCspInDevPlugin } from "#libs/vite-plugin-disable-csp/plugin.ts";

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
    genJarmnPlugin(
      "content-profile-manager",
      "noraneko-profile-manager",
      "content",
    ),
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
