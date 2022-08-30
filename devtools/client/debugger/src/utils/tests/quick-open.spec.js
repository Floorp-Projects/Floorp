/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import cases from "jest-in-case";
import { parseQuickOpenQuery, parseLineColumn } from "../quick-open";

cases(
  "parseQuickOpenQuery utility",
  ({ type, query }) => expect(parseQuickOpenQuery(query)).toEqual(type),
  [
    { name: "empty query defaults to sources", type: "sources", query: "" },
    { name: "sources query", type: "sources", query: "test" },
    { name: "functions query", type: "functions", query: "@test" },
    { name: "variables query", type: "variables", query: "#test" },
    { name: "goto line", type: "goto", query: ":30" },
    { name: "goto line:column", type: "goto", query: ":30:60" },
    { name: "goto source line", type: "gotoSource", query: "test:30:60" },
    { name: "shortcuts", type: "shortcuts", query: "?" },
  ]
);

cases(
  "parseLineColumn utility",
  ({ query, location }) => expect(parseLineColumn(query)).toEqual(location),
  [
    { name: "empty query", query: "", location: null },
    { name: "just line", query: ":30", location: { line: 30 } },
    {
      name: "line and column",
      query: ":30:90",
      location: { column: 90, line: 30 },
    },
  ]
);
