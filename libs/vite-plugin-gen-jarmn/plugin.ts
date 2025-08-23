/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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