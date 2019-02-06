/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import fs from "fs";
import path from "path";

import { makeMockSource } from "../../../../utils/test-mockup";

export function getFixture(name: string, type: string = "js") {
  return fs.readFileSync(
    path.join(__dirname, `../fixtures/${name}.${type}`),
    "utf8"
  );
}

export function getSource(name: string, type: string = "js") {
  const text = getFixture(name, type);
  let contentType = "text/javascript";
  if (type === "html") {
    contentType = "text/html";
  } else if (type === "vue") {
    contentType = "text/vue";
  } else if (type === "ts") {
    contentType = "text/typescript";
  } else if (type === "tsx") {
    contentType = "text/typescript-jsx";
  }

  return makeMockSource(undefined, name, contentType, text);
}

export function getOriginalSource(name: string, type: string = "js") {
  const source = getSource(name, type);
  return { ...source, id: `${name}/originalSource-1` };
}
