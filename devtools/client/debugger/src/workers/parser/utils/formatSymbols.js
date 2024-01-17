/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSymbols } from "../getSymbols";

function formatLocation(loc) {
  if (!loc) {
    return "";
  }

  const { start, end } = loc;
  const startLoc = `(${start.line}, ${start.column})`;
  const endLoc = `(${end.line}, ${end.column})`;

  return `[${startLoc}, ${endLoc}]`;
}

function summarize(symbol) {
  if (typeof symbol == "boolean") {
    return symbol ? "true" : "false";
  }

  const loc = formatLocation(symbol.location);
  const params = symbol.parameterNames
    ? `(${symbol.parameterNames.join(", ")})`
    : "";
  const expression = symbol.expression || "";
  const klass = symbol.klass || "";
  const name = symbol.name == undefined ? "" : symbol.name;
  const names = symbol.specifiers ? symbol.specifiers.join(", ") : "";
  const values = symbol.values ? symbol.values.join(", ") : "";
  const index = symbol.index ? symbol.index : "";

  return `${loc} ${expression} ${name}${params} ${klass} ${names} ${values} ${index}`.trim(); // eslint-disable-line max-len
}
const bools = ["hasJsx", "hasTypes"];
function formatBool(name, symbols) {
  return `${name}: ${symbols[name] ? "true" : "false"}`;
}

function formatKey(name, symbols) {
  if (bools.includes(name)) {
    return formatBool(name, symbols);
  }

  return `${name}:\n${symbols[name].map(summarize).join("\n")}`;
}

export function formatSymbols(sourceId) {
  const symbols = getSymbols(sourceId);

  return Object.keys(symbols)
    .map(name => formatKey(name, symbols))
    .join("\n\n");
}
