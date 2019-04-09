/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getAst } from "../ast";
import { setSource } from "../../sources";
import cases from "jest-in-case";

import { makeMockSource } from "../../../../utils/test-mockup";

function createSource(contentType) {
  return makeMockSource(undefined, "foo", contentType, "2");
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
    const ast = getAst("foo");
    expect(ast && Object.keys(ast)).toEqual(astKeys);
  },
  [
    { name: "text/javascript" },
    { name: "application/javascript" },
    { name: "application/x-javascript" },
    { name: "text/jsx" }
  ]
);
