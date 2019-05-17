/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import classnames from "classnames";
import { endTruncateStr } from "./utils";
import {
  isPretty,
  getFilename,
  getSourceClassnames,
  getSourceQueryString,
} from "./source";

import type { Location as BabelLocation } from "@babel/types";
import type { Symbols } from "../reducers/ast";
import type { QuickOpenType } from "../reducers/quick-open";
import type { Tab } from "../reducers/tabs";
import type { Source } from "../types";
import type {
  SymbolDeclaration,
  IdentifierDeclaration,
} from "../workers/parser";

export const MODIFIERS = {
  "@": "functions",
  "#": "variables",
  ":": "goto",
  "?": "shortcuts",
};

export function parseQuickOpenQuery(query: string): QuickOpenType {
  const modifierPattern = /^@|#|:|\?$/;
  const gotoSourcePattern = /^(\w+)\:/;
  const startsWithModifier = modifierPattern.test(query[0]);
  const isGotoSource = gotoSourcePattern.test(query);

  if (startsWithModifier) {
    const modifier = query[0];
    return MODIFIERS[modifier];
  }

  if (isGotoSource) {
    return "gotoSource";
  }

  return "sources";
}

export function parseLineColumn(query: string) {
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

export function formatSourcesForList(
  source: Source,
  tabUrls: Set<$PropertyType<Tab, "url">>
) {
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
      : classnames(getSourceClassnames(source), "result-item-icon"),
    id: source.id,
    url: source.url,
  };
}

export type QuickOpenResult = {|
  id: string,
  value: string,
  title: string | React$Element<"div">,
  subtitle?: string,
  location?: BabelLocation,
  url?: string,
  icon?: string,
|};

export type FormattedSymbolDeclarations = {|
  functions: Array<QuickOpenResult>,
|};

export function formatSymbol(
  symbol: SymbolDeclaration | IdentifierDeclaration
): QuickOpenResult {
  return {
    id: `${symbol.name}:${symbol.location.start.line}`,
    title: symbol.name,
    subtitle: `${symbol.location.start.line}`,
    value: symbol.name,
    location: symbol.location,
  };
}

export function formatSymbols(symbols: ?Symbols): FormattedSymbolDeclarations {
  if (!symbols || symbols.loading) {
    return { functions: [] };
  }

  const { functions } = symbols;

  return {
    functions: functions.map(formatSymbol),
  };
}

export function formatShortcutResults(): Array<QuickOpenResult> {
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

export function formatSources(
  sources: Source[],
  tabUrls: Set<$PropertyType<Tab, "url">>
): Array<QuickOpenResult> {
  return sources
    .filter(source => !isPretty(source))
    .filter(source => !!source.relativeUrl && !isPretty(source))
    .map(source => formatSourcesForList(source, tabUrls));
}
