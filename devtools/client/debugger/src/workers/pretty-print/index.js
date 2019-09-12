/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

let dispatcher;
let workerPath;

export const start = (path: string) => {
  workerPath = path;
};
export const stop = () => {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
    workerPath = null;
  }
};

type PrettyPrintOpts = {
  text: string,
  url: string,
};

export async function prettyPrint({ text, url }: PrettyPrintOpts) {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(workerPath);
  }

  return dispatcher.invoke("prettyPrint", {
    url,
    indent: 2,
    sourceText: text,
  });
}
