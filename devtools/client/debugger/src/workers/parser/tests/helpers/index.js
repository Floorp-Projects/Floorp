/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import fs from "fs";
import path from "path";

import { makeMockSourceAndContent } from "../../../../utils/test-mockup";
import { setSource } from "../../sources";
import * as asyncValue from "../../../../utils/async-value";

export function getFixture(name, type = "js") {
  return fs.readFileSync(
    path.join(__dirname, `../fixtures/${name}.${type}`),
    "utf8"
  );
}

function getSourceContent(name, type = "js") {
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

  return {
    type: "text",
    value: text,
    contentType,
  };
}

export function getSource(name, type) {
  const { value: text, contentType } = getSourceContent(name, type);

  return makeMockSourceAndContent(undefined, name, contentType, text);
}

export function populateSource(name, type) {
  const { content, ...source } = getSource(name, type);
  setSource({
    id: source.id,
    text: content.value,
    contentType: content.contentType,
    isWasm: false,
  });
  return {
    ...source,
    content: asyncValue.fulfilled(content),
  };
}

export function getOriginalSource(name, type) {
  return getOriginalSourceWithContent(name, type);
}

export function getOriginalSourceWithContent(name, type) {
  const { value: text, contentType } = getSourceContent(name, type);

  return makeMockSourceAndContent(
    undefined,
    `${name}/originalSource-1`,
    contentType,
    text
  );
}

export function populateOriginalSource(name, type) {
  const { content, ...source } = getOriginalSourceWithContent(name, type);
  setSource({
    id: source.id,
    text: content.value,
    contentType: content.contentType,
    isWasm: false,
  });
  return {
    ...source,
    content: asyncValue.fulfilled(content),
  };
}
