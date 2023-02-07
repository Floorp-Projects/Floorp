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

  // We need to swap original and generated locations, as the prettified text should
  // be seen by the sourcemap service as the "original" one.
  const mappingLength = sourceMapGenerator._mappings._array.length;
  for (let i = 0; i < mappingLength; i++) {
    const mapping = sourceMapGenerator._mappings._array[i];
    const {
      originalLine,
      originalColumn,
      generatedLine,
      generatedColumn,
    } = mapping;
    mapping.originalLine = generatedLine;
    mapping.originalColumn = generatedColumn;
    mapping.generatedLine = originalLine;
    mapping.generatedColumn = originalColumn;
  }
  // Since we modified the location, the mappings might not be in the expected order,
  // which may cause issues when generating the sourceMap.
  // Flip the `_sorted` flag so the mappings will be sorted when the sourceMap is built.
  sourceMapGenerator._mappings._sorted = false;

  return {
    code,
    sourceMap: sourceMapGenerator.toJSON(),
  };
}

self.onmessage = workerHandler({ prettyPrint });
