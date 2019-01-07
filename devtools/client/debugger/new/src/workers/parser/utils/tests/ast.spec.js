/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getAst } from "../ast";
import { setSource } from "../../sources";
import cases from "jest-in-case";

function createSource(contentType) {
  return { id: "foo", text: "2", contentType };
}

const astKeys = [
  "type",
  "start",
  "end",
  "loc",
  "program",
  "comments",
  "tokens"
];

cases(
  "ast.getAst",
  ({ name }) => {
    const source = createSource(name);
    setSource(source);
    expect(Object.keys(getAst("foo"))).toEqual(astKeys);
  },
  [
    { name: "text/javascript" },
    { name: "application/javascript" },
    { name: "application/x-javascript" },
    { name: "text/jsx" }
  ]
);
