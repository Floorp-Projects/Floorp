/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import fs from "fs";
import path from "path";

import type {
  Source,
  TextSourceContent,
  SourceWithContent,
} from "../../../../types";
import { makeMockSourceAndContent } from "../../../../utils/test-mockup";
import { setSource } from "../../sources";
import * as asyncValue from "../../../../utils/async-value";

export function getFixture(name: string, type: string = "js") {
  return fs.readFileSync(
    path.join(__dirname, `../fixtures/${name}.${type}`),
    "utf8"
  );
}

function getSourceContent(
  name: string,
  type: string = "js"
): TextSourceContent {
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

export function getSource(name: string, type?: string): Source {
  return getSourceWithContent(name, type).source;
}

export function getSourceWithContent(
  name: string,
  type?: string
): { source: Source, content: TextSourceContent } {
  const { value: text, contentType } = getSourceContent(name, type);

  return makeMockSourceAndContent(undefined, name, contentType, text);
}

export function populateSource(name: string, type?: string): SourceWithContent {
  const { source, content } = getSourceWithContent(name, type);
  setSource({
    id: source.id,
    text: content.value,
    contentType: content.contentType,
    isWasm: false,
  });
  return {
    source,
    content: asyncValue.fulfilled(content),
  };
}

export function getOriginalSource(name: string, type?: string): Source {
  return getOriginalSourceWithContent(name, type).source;
}

export function getOriginalSourceWithContent(
  name: string,
  type?: string
): { source: Source, content: TextSourceContent } {
  const { value: text, contentType } = getSourceContent(name, type);

  return makeMockSourceAndContent(
    undefined,
    `${name}/originalSource-1`,
    contentType,
    text
  );
}

export function populateOriginalSource(
  name: string,
  type?: string
): SourceWithContent {
  const { source, content } = getOriginalSourceWithContent(name, type);
  setSource({
    id: source.id,
    text: content.value,
    contentType: content.contentType,
    isWasm: false,
  });
  return {
    source,
    content: asyncValue.fulfilled(content),
  };
}
