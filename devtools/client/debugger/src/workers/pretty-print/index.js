/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../../utils/prefs";
import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

let dispatcher;
let workerPath;

export const start = path => {
  workerPath = path;
};
export const stop = () => {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
    workerPath = null;
  }
};

export async function prettyPrint({ text, url }) {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(workerPath);
  }

  return dispatcher.invoke("prettyPrint", {
    url,
    indent: prefs.indentSize,
    sourceText: text,
  });
}
