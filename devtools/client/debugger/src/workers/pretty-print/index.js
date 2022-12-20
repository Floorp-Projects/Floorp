/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../../utils/prefs";
import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const WORKER_URL =
  "resource://devtools/client/debugger/dist/pretty-print-worker.js";

export class PrettyPrintDispatcher extends WorkerDispatcher {
  constructor(jestUrl) {
    super(jestUrl || WORKER_URL);
  }

  #prettyPrintTask = this.task("prettyPrint");

  prettyPrint({ url, text }) {
    return this.#prettyPrintTask({
      url,
      indent: prefs.indentSize,
      sourceText: text,
    });
  }
}
