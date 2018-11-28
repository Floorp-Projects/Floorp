/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  type SymbolDeclaration,
  type SymbolDeclarations
} from "../../workers/parser";

import type { Source } from "../../types";

function updateSymbolLocation(
  site: SymbolDeclaration,
  source: Source,
  sourceMaps: any
) {
  return sourceMaps
    .getGeneratedLocation(
      { ...site.location.start, sourceId: source.id },
      source
    )
    .then(loc => {
      return {
        ...site,
        generatedLocation: { line: loc.line, column: loc.column }
      };
    });
}

export async function updateSymbolLocations(
  symbols: SymbolDeclarations,
  source: Source,
  sourceMaps: any
): Promise<SymbolDeclarations> {
  if (!symbols || !symbols.callExpressions) {
    return Promise.resolve(symbols);
  }

  const mappedCallExpressions = await Promise.all(
    symbols.callExpressions.map(site =>
      updateSymbolLocation(site, source, sourceMaps)
    )
  );

  const newSymbols = { ...symbols, callExpressions: mappedCallExpressions };

  return Promise.resolve(newSymbols);
}
