/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import prettyFast from "pretty-fast";

import { workerUtils } from "devtools-utils";
const { workerHandler } = workerUtils;

type Mappings = {
  _array: Mapping[],
};

type Mapping = {
  originalLine: number,
  originalColumn: number,
  source?: string,
  generatedLine?: number,
  generatedColumn?: number,
  name?: string,
};

type InvertedMapping = {
  generated: Object,
  source?: any,
  original?: any,
  name?: string,
};

function prettyPrint({ url, indent, sourceText }) {
  const prettified = prettyFast(sourceText, {
    url: url,
    indent: " ".repeat(indent),
  });

  return {
    code: prettified.code,
    mappings: invertMappings(prettified.map._mappings),
  };
}

function invertMappings(mappings: Mappings) {
  return mappings._array.map((m: Mapping) => {
    const mapping: InvertedMapping = {
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
