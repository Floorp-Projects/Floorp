/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import prettyFast from "pretty-fast";

import { workerUtils } from "devtools-utils";
const { workerHandler } = workerUtils;

function prettyPrint({ url, indent, sourceText }) {
  const prettified = prettyFast(sourceText, {
    url,
    indent: " ".repeat(indent),
  });

  return {
    code: prettified.code,
    mappings: invertMappings(prettified.map._mappings),
  };
}

function invertMappings(mappings) {
  return mappings._array.map(m => {
    const mapping = {
      generated: {
        line: m.originalLine,
        column: m.originalColumn,
      },
    };
    if (m.source) {
      mapping.source = m.source;
      mapping.original = {
        line: m.generatedLine,
        column: m.generatedColumn,
      };
      mapping.name = m.name;
    }
    return mapping;
  });
}

self.onmessage = workerHandler({ prettyPrint });
