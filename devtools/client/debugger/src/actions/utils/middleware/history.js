/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isDevelopment } from "devtools-environment";

import type { ThunkArgs } from "../../types";

/**
 * A middleware that stores every action coming through the store in the passed
 * in logging object. Should only be used for tests, as it collects all
 * action information, which will cause memory bloat.
 */
export const history = (log: Object[] = []) => ({
  dispatch,
  getState,
}: ThunkArgs) => {
  return (next: Function) => (action: Object) => {
    if (isDevelopment()) {
      log.push(action);
    }

    return next(action);
  };
};
