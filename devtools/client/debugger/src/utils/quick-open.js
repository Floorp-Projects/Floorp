/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { endTruncateStr } from "./utils";
import {
  isPretty,
  getFilename,
  getSourceClassnames,
  getSourceQueryString,
} from "./source";

export const MODIFIERS = {
  "@": "functions",
  "#": "variables",
  ":": "goto",
  "?": "shortcuts",
};

export function parseQuickOpenQuery(query) {
  const startsWithModifier =
    query[0] === "@" ||
    query[0] === "#" ||
    query[0] === ":" ||
    query[0] === "?";

  if (startsWithModifier) {
    const modifier = query[0];
    return MODIFIERS[modifier];
  }

  const isGotoSource = query.includes(":", 1);

  if (isGotoSource) {
    return "gotoSource";
  }

  return "sources";
}

export function parseLineColumn(query) {
  const [, line, column] = query.split(":");
  const lineNumber = parseInt(line, 10);
  const columnNumber = parseInt(column, 10);
  if (!isNaN(lineNumber)) {
    return {
      line: lineNumber,
      ...(!isNaN(columnNumber) ? { column: columnNumber } : null),
    };
  }
}

export function formatSourcesForList(source, tabUrls) {
  const title = getFilename(source);
  const relativeUrlWithQuery = `${source.relativeUrl}${getSourceQueryString(
    source
  ) || ""}`;
  const subtitle = endTruncateStr(relativeUrlWithQuery, 100);
  const value = relativeUrlWithQuery;
  return {
    value,
    title,
    subtitle,
    icon: tabUrls.has(source.url)
      ? "tab result-item-icon"
      : `result-item-icon ${getSourceClassnames(source)}`,
    id: source.id,
    url: source.url,
  };
}

export function formatSymbol(symbol) {
  return {
    id: `${symbol.name}:${symbol.location.start.line}`,
    title: symbol.name,
    subtitle: `${symbol.location.start.line}`,
    value: symbol.name,
    location: symbol.location,
  };
}

export function formatSymbols(symbols) {
  if (!symbols || symbols.loading) {
    return { functions: [] };
  }

  const { functions } = symbols;

  return {
    functions: functions.map(formatSymbol),
  };
}

export function formatShortcutResults() {
  return [
    {
      value: L10N.getStr("symbolSearch.search.functionsPlaceholder.title"),
      title: `@ ${L10N.getStr("symbolSearch.search.functionsPlaceholder")}`,
      id: "@",
    },
    {
      value: L10N.getStr("symbolSearch.search.variablesPlaceholder.title"),
      title: `# ${L10N.getStr("symbolSearch.search.variablesPlaceholder")}`,
      id: "#",
    },
    {
      value: L10N.getStr("gotoLineModal.title"),
      title: `: ${L10N.getStr("gotoLineModal.placeholder")}`,
      id: ":",
    },
  ];
}

export function formatSources(sources, tabUrls) {
  const formattedSources = [];

  for (let i = 0; i < sources.length; ++i) {
    const source = sources[i];

    if (!!source.relativeUrl && !isPretty(source)) {
      formattedSources.push(formatSourcesForList(source, tabUrls));
    }
  }

  return formattedSources;
}
