// SPDX-License-Identifier: MPL-2.0

import {generateJarManifest} from "./gen_jarmanifest.ts"
import type { Plugin } from "rolldown";

export function genJarmnPlugin(prefix:string,namespace:string,register_type:"content"|"skin"|"resource") {
  return {
    name: "gen_jarmn",
    async generateBundle(options, bundle, isWrite) {
      this.emitFile({
        type: "asset",
        fileName: "jar.mn",
        source: await generateJarManifest(bundle, {
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
  } satisfies Plugin
};