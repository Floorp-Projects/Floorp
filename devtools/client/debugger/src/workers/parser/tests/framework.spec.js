/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSymbols } from "../getSymbols";
import { populateOriginalSource } from "./helpers";
import cases from "jest-in-case";

cases(
  "Parser.getFramework",
  ({ name, file, value }) => {
    const { source } = populateOriginalSource("frameworks/plainJavascript");
    const symbols = getSymbols(source.id);
    expect(symbols.framework).toBeUndefined();
  },
  [
    {
      name: "is undefined when no framework",
      file: "frameworks/plainJavascript",
      value: undefined,
    },
    {
      name: "does not get confused with angular (#6833)",
      file: "frameworks/angular1FalsePositive",
      value: undefined,
    },
    {
      name: "recognizes ES6 React component",
      file: "frameworks/reactComponent",
      value: "React",
    },
    {
      name: "recognizes ES5 React component",
      file: "frameworks/reactComponentEs5",
      value: "React",
    },
    {
      name: "recognizes Angular 1 module",
      file: "frameworks/angular1Module",
      value: "Angular",
    },
    {
      name: "recognizes declarative Vue file",
      file: "frameworks/vueFileDeclarative",
      value: "Vue",
    },
    {
      name: "recognizes component Vue file",
      file: "frameworks/vueFileComponent",
      value: "Vue",
    },
    {
      name: "recognizes the react library file",
      file: "framework/reactLibrary",
      value: "React",
    },
    {
      name: "recognizes the redux library file",
      file: "framework/reduxLibrary",
      value: "React",
    },
  ]
);
