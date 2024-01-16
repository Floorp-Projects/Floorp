/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const WORKER_URL = "resource://devtools/client/debugger/dist/parser-worker.js";

export class ParserDispatcher extends WorkerDispatcher {
  constructor(jestUrl) {
    super(jestUrl || WORKER_URL);
  }

  findOutOfScopeLocations = this.task("findOutOfScopeLocations");
  findBestMatchExpression = this.task("findBestMatchExpression");

  getScopes = this.task("getScopes");

  getSymbols = this.task("getSymbols");
  getFunctionSymbols = this.task("getFunctionSymbols");
  getClassSymbols = this.task("getClassSymbols");
  getClosestFunctionName = this.task("getClosestFunctionName");

  async setSource(sourceId, content) {
    const astSource = {
      id: sourceId,
      text: content.type === "wasm" ? "" : content.value,
      contentType: content.contentType || null,
      isWasm: content.type === "wasm",
    };

    return this.invoke("setSource", astSource);
  }

  hasSyntaxError = this.task("hasSyntaxError");

  mapExpression = this.task("mapExpression");

  clearSources = this.task("clearSources");

  /**
   * Reports if the location's source can be parsed by this worker.
   *
   * @param {Object} location
   *        A debugger frontend location object. See createLocation().
   * @return {Boolean}
   *         True, if the worker may be able to parse this source.
   */
  isLocationSupported(location) {
    // There might be more sources that the worker doesn't support,
    // like original sources which aren't JavaScript.
    // But we can only know with the source's content type,
    // which isn't available right away.
    // These source will be ignored from within the worker.
    return !location.source.isWasm;
  }
}
