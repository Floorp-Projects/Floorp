import { defineConfig } from "vite";

export default defineConfig({
  server: {
    port: 5182,
  },
  build: {
    outDir: "_dist",
    target: "firefox128",
    reportCompressedSize: false,
    rollupOptions: {
      input: {
        designs: "./src/index.ts",
      },
    },
  },
});
