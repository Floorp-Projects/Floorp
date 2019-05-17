/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSymbols, clearSymbols } from "./getSymbols";
import { clearASTs } from "./utils/ast";
import getScopes, { clearScopes } from "./getScopes";
import { setSource, clearSources } from "./sources";
import findOutOfScopeLocations from "./findOutOfScopeLocations";
import { getNextStep } from "./steps";
import { hasSyntaxError } from "./validate";
import mapExpression from "./mapExpression";

import { workerUtils } from "devtools-utils";
const { workerHandler } = workerUtils;

function clearState() {
  clearASTs();
  clearScopes();
  clearSources();
  clearSymbols();
}

self.onmessage = workerHandler({
  findOutOfScopeLocations,
  getSymbols,
  getScopes,
  clearState,
  getNextStep,
  hasSyntaxError,
  mapExpression,
  setSource,
});
