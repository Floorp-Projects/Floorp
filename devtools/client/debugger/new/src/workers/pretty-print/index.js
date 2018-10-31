/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;
import { isJavaScript } from "../../utils/source";
import assert from "../../utils/assert";

import type { Source } from "../../types";

const dispatcher = new WorkerDispatcher();
export const start = dispatcher.start.bind(dispatcher);
export const stop = dispatcher.stop.bind(dispatcher);
const _prettyPrint = dispatcher.task("prettyPrint");

type PrettyPrintOpts = {
  source: Source,
  url: string
};

export async function prettyPrint({ source, url }: PrettyPrintOpts) {
  const indent = 2;

  assert(isJavaScript(source), "Can't prettify non-javascript files.");

  return await _prettyPrint({
    url,
    indent,
    sourceText: source.text
  });
}
