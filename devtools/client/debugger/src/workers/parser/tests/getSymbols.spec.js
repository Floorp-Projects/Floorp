/* eslint max-nested-callbacks: ["error", 4]*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { formatSymbols } from "../utils/formatSymbols";
import { populateSource, populateOriginalSource } from "./helpers";
import cases from "jest-in-case";

cases(
  "Parser.getSymbols",
  ({ name, file, original, type }) => {
    const { source } = original
      ? populateOriginalSource(file, type)
      : populateSource(file, type);

    expect(formatSymbols(source.id)).toMatchSnapshot();
  },
  [
    { name: "es6", file: "es6", original: true },
    { name: "func", file: "func", original: true },
    { name: "function names", file: "functionNames", original: true },
    { name: "math", file: "math" },
    { name: "proto", file: "proto" },
    { name: "class", file: "class", original: true },
    { name: "var", file: "var" },
    { name: "expression", file: "expression" },
    { name: "allSymbols", file: "allSymbols" },
    { name: "call sites", file: "call-sites" },
    { name: "call expression", file: "callExpressions" },
    { name: "object expressions", file: "object-expressions" },
    {
      name: "finds symbols in an html file",
      file: "parseScriptTags",
      type: "html",
    },
    { name: "component", file: "component", original: true },
    {
      name: "react component",
      file: "frameworks/reactComponent",
      original: true,
    },
    { name: "flow", file: "flow", original: true },
    { name: "jsx", file: "jsx", original: true },
    { name: "destruct", file: "destructuring" },
  ]
);
