// SPDX-License-Identifier: MPL-2.0

import { generateJarManifest } from "./gen_jarmanifest.ts";
import type { Plugin } from "rolldown";
import fs from "node:fs";

export function genJarmnPlugin(
  prefix: string,
  namespace: string,
  register_type: "content" | "skin" | "resource",
) {
  let rootPath = "";
  return {
    name: "gen_jarmn",
    configResolved(config) {
      rootPath = config.root;
    },
    async generateBundle(options, bundle, isWrite) {
      const _bundle = fs.existsSync(rootPath + "/index.html")
        ? Object.assign(
            { "__index.html__": { fileName: "index.html" } },
            bundle,
          )
        : bundle;
      this.emitFile({
        type: "asset",
        fileName: "jar.mn",
        source: await generateJarManifest(_bundle, {
          prefix,
          namespace,
          register_type,
        }),
      });
      this.emitFile({
        type: "asset",
        fileName: "moz.build",
        source: `JAR_MANIFESTS += ["jar.mn"]`,
      });
    },
  } satisfies Plugin;
}
