/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

const dispatcher = new WorkerDispatcher();
export const start = dispatcher.start.bind(dispatcher);
export const stop = dispatcher.stop.bind(dispatcher);

type PrettyPrintOpts = {
  text: string,
  url: string,
};

export async function prettyPrint({ text, url }: PrettyPrintOpts) {
  return dispatcher.invoke("prettyPrint", {
    url,
    indent: 2,
    sourceText: text,
  });
}
