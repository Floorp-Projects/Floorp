import { defineConfig } from "vite";
import path from "node:path";
import tsconfigPaths from "vite-tsconfig-paths";
import solidPlugin from "vite-plugin-solid";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

// export class StringAsBytes {
//   private string: Uint8Array;
//   private decoder: TextDecoder;

//   constructor(string: string) {
//     this.decoder = new TextDecoder();
//     this.string = new TextEncoder().encode(string);
//   }

//   /**
//    * Returns a slice of the string by providing byte indices.
//    * @param from - Byte index to slice from
//    * @param to - Optional byte index to slice to
//    */
//   public slice(from: number, to?: number): string {
//     return this.decoder.decode(
//       new DataView(
//         this.string.buffer,
//         from,
//         to !== undefined ? to - from : undefined,
//       ),
//     );
//   }
// }

export default defineConfig({
  publicDir: r("public"),
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    modulePreload: false,

    rollupOptions: {
      //https://github.com/vitejs/vite/discussions/14454
      preserveEntrySignatures: "allow-extension",
      input: {
        browser: "./startup/browser/index.ts",
        preferences: "./startup/preferences/index.ts",
      },
      output: {
        esModule: true,
        entryFileNames: "content/[name].js",
      },
    },

    outDir: r("_dist"),
    assetsDir: "content/assets",
  },
  //? https://github.com/parcel-bundler/lightningcss/issues/685
  //? lepton uses System Color and that occurs panic.
  //? when the issue resolved, gladly we can use lightningcss
  // css: {
  //   transformer: "lightningcss",
  // },

  plugins: [
    // tsconfigPaths(),
    solidPlugin({
      solid: {
        generate: "universal",
        moduleName: r("./solid-xul"),
      },
    }),
    // {
    //   name: "test",
    //   apply: "serve",
    //   transform: {
    //     handler: (code, id, options) => {
    //       console.log(id);
    //       const result = swc.transformSync(code, {
    //         jsc: {
    //           target: "esnext",
    //           experimental: {
    //             plugins: [
    //               [
    //                 "@nora/noraneko-hmr-transformer",
    //                 {
    //                   removeImport: id.includes("@nora/vite-override")
    //                     ? "/@vite/client"
    //                     : null,
    //                 },
    //               ],
    //             ],
    //           },
    //         },
    //         sourceMaps: true,
    //       });
    //       //https://github.com/vitejs/vite/blob/22b299429599834bf1855b53264a28ae5ff8f888/packages/vite/src/node/plugins/clientInjections.ts#L89
    //       return {
    //         code: result.code,
    //         map: JSON.parse(result.map!),
    //       };
    //     },
    //     order: "post",
    //   },
    // },
    // clientInjectionsPlugin(),
  ],
  resolve: {
    preserveSymlinks: true,
    alias: [
      {
        find: "@nora/solid-xul",
        replacement: r("./solid-xul"),
      },
    ],
    // alias: [
    //   {
    //     //https://github.com/vitejs/vite/blob/22b299429599834bf1855b53264a28ae5ff8f888/packages/vite/src/node/config.ts#L537C13-L537C32
    //     find: /^\/?@vite\/client/,
    //     replacement: r("node_modules/@nora/vite-override/client"),
    //   },
    // ],
  },
});
