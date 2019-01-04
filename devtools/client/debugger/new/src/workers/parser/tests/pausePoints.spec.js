/* eslint max-nested-callbacks: ["error", 4]*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import cases from "jest-in-case";
import {
  formatPausePoints,
  convertToList
} from "../../../utils/pause/pausePoints";

import { getPausePoints } from "../pausePoints";
import { getSource, getOriginalSource } from "./helpers";
import { setSource } from "../sources";

cases(
  "Parser.pausePoints",
  ({ name, file, original, type }) => {
    const source = original
      ? getOriginalSource(file, type)
      : getSource(file, type);

    setSource(source);
    const nodes = convertToList(getPausePoints(source.id));
    expect(formatPausePoints(source.text, nodes)).toMatchSnapshot();
  },
  [
    { name: "control-flow", file: "control-flow" },
    { name: "flow", file: "flow", original: true },
    { name: "calls", file: "calls" },
    { name: "statements", file: "statements" },
    { name: "modules", file: "modules", original: true },
    { name: "jsx", file: "jsx", original: true },
    { name: "func", file: "func", original: true },
    { name: "decorators", file: "decorators", original: true },
    { name: "html", file: "parseScriptTags", type: "html" }
  ]
);
