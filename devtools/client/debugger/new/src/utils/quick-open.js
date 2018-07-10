"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.MODIFIERS = undefined;
exports.parseQuickOpenQuery = parseQuickOpenQuery;
exports.parseLineColumn = parseLineColumn;
exports.formatSourcesForList = formatSourcesForList;
exports.formatSymbol = formatSymbol;
exports.formatSymbols = formatSymbols;
exports.formatShortcutResults = formatShortcutResults;
exports.formatSources = formatSources;

var _classnames = require("devtools/client/debugger/new/dist/vendors").vendored["classnames"];

var _classnames2 = _interopRequireDefault(_classnames);

var _utils = require("./utils");

var _source = require("./source");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const MODIFIERS = exports.MODIFIERS = {
  "@": "functions",
  "#": "variables",
  ":": "goto",
  "?": "shortcuts"
};

function parseQuickOpenQuery(query) {
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

function parseLineColumn(query) {
  const [, line, column] = query.split(":");
  const lineNumber = parseInt(line, 10);
  const columnNumber = parseInt(column, 10);

  if (!isNaN(lineNumber)) {
    return {
      line: lineNumber,
      ...(!isNaN(columnNumber) ? {
        column: columnNumber
      } : null)
    };
  }
}

function formatSourcesForList(source, tabs) {
  const title = (0, _source.getFilename)(source);
  const subtitle = (0, _utils.endTruncateStr)(source.relativeUrl, 100);
  return {
    value: source.relativeUrl,
    title,
    subtitle,
    icon: tabs.includes(source.url) ? "tab result-item-icon" : (0, _classnames2.default)((0, _source.getSourceClassnames)(source), "result-item-icon"),
    id: source.id,
    url: source.url
  };
}

function formatSymbol(symbol) {
  return {
    id: `${symbol.name}:${symbol.location.start.line}`,
    title: symbol.name,
    subtitle: `${symbol.location.start.line}`,
    value: symbol.name,
    location: symbol.location
  };
}

function formatSymbols(symbols) {
  if (!symbols || symbols.loading) {
    return {
      variables: [],
      functions: []
    };
  }

  const {
    variables,
    functions
  } = symbols;
  return {
    variables: variables.map(formatSymbol),
    functions: functions.map(formatSymbol)
  };
}

function formatShortcutResults() {
  return [{
    value: L10N.getStr("symbolSearch.search.functionsPlaceholder.title"),
    title: `@ ${L10N.getStr("symbolSearch.search.functionsPlaceholder")}`,
    id: "@"
  }, {
    value: L10N.getStr("symbolSearch.search.variablesPlaceholder.title"),
    title: `# ${L10N.getStr("symbolSearch.search.variablesPlaceholder")}`,
    id: "#"
  }, {
    value: L10N.getStr("gotoLineModal.title"),
    title: `: ${L10N.getStr("gotoLineModal.placeholder")}`,
    id: ":"
  }];
}

function formatSources(sources, tabs) {
  const sourceList = Object.values(sources);
  return sourceList.filter(source => !(0, _source.isPretty)(source)).filter(({
    relativeUrl
  }) => !!relativeUrl).map(source => formatSourcesForList(source, tabs));
}