/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../../utils/prefs";
import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const WORKER_URL =
  "resource://devtools/client/debugger/dist/pretty-print-worker.js";

let dispatcher;
let jestWorkerUrl;

export const start = jestUrl => {
  jestWorkerUrl = jestUrl;
};
export const stop = () => {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
  }
};

export async function prettyPrint({ text, url }) {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(jestWorkerUrl || WORKER_URL);
  }

  return dispatcher.invoke("prettyPrint", {
    url,
    indent: prefs.indentSize,
    sourceText: text,
  });
}
