/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import fs from "fs";
import path from "path";

import type { Source } from "../../../../types";
import { makeMockSource } from "../../../../utils/test-mockup";
import { setSource } from "../../sources";

export function getFixture(name: string, type: string = "js") {
  return fs.readFileSync(
    path.join(__dirname, `../fixtures/${name}.${type}`),
    "utf8"
  );
}

export function getSource(name: string, type: string = "js"): Source {
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

export function getOriginalSource(name: string, type: string = "js"): Source {
  const source = getSource(name, type);
  return ({ ...source, id: `${name}/originalSource-1` }: any);
}

export function populateSource(name: string, type?: string): Source {
  const source = getSource(name, type);
  setSource({
    id: source.id,
    text: source.isWasm ? "" : source.text || "",
    contentType: source.contentType,
    isWasm: false
  });
  return source;
}

export function populateOriginalSource(name: string, type?: string): Source {
  const source = getOriginalSource(name, type);
  setSource({
    id: source.id,
    text: source.isWasm ? "" : source.text || "",
    contentType: source.contentType,
    isWasm: false
  });
  return source;
}
