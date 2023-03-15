/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { workerHandler } from "devtools/client/shared/worker-utils";
import { prettyFast } from "./pretty-fast";

function prettyPrint({ url, indent, sourceText }) {
  const { code, map: sourceMapGenerator } = prettyFast(sourceText, {
    url,
    indent: " ".repeat(indent),
  });

  return {
    code,
    sourceMap: sourceMapGenerator.toJSON(),
  };
}

self.onmessage = workerHandler({ prettyPrint });
