/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getAst } from "../ast";
import { setSource } from "../../sources";
import cases from "jest-in-case";

import { makeMockSourceAndContent } from "../../../../utils/test-mockup";

const astKeys = [
  "type",
  "start",
  "end",
  "loc",
  "program",
  "comments",
  "tokens",
];

cases(
  "ast.getAst",
  ({ name }) => {
    const source = makeMockSourceAndContent(undefined, "foo", name, "2");
    setSource({
      id: source.id,
      text: source.content.value || "",
      contentType: source.content.contentType,
      isWasm: false,
    });
    const ast = getAst("foo");
    expect(ast && Object.keys(ast)).toEqual(astKeys);
  },
  [
    { name: "text/javascript" },
    { name: "application/javascript" },
    { name: "application/x-javascript" },
    { name: "text/jsx" },
  ]
);
