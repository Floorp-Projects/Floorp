import { defineConfig } from "tsdown";
import { relative } from "pathe";
import { generateJarManifest } from "../../common/scripts/gen_jarmanifest.ts";
import { getEntries } from "../../modules/mod.ts";

export default defineConfig({
  entry: await getEntries(),
  target: "esnext",
  outDir: "_dist",
  external: /^resource:\/\/|^chrome:\/\//g,
  plugins: [{
    name: "gen_jarmn",
    enforce: "post",
    async generateBundle(options, bundle, isWrite) {
      this.emitFile({
        type: "asset",
        fileName: "jar.mn",
        needsCodeReference: false,
        source: await generateJarManifest(bundle, {
          prefix: "content",
          namespace: "noraneko",
          register_type: "content",
        }),
      });
      this.emitFile({
        type: "asset",
        fileName: "moz.build",
        needsCodeReference: false,
        source: `JAR_MANIFESTS += ["jar.mn"]`,
      });
    },
  }],
});
