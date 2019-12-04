/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type {
  Grip,
  ObjectFront,
  LongStringFront,
  ExpressionResult,
} from "../client/firefox/types";

function isFront(result: ExpressionResult): boolean %checks {
  return !!result && typeof result === "object" && !!result.getGrip;
}

export function getGrip(
  result: ExpressionResult
): Grip | string | number | null {
  if (isFront(result)) {
    return result.getGrip();
  }

  return result;
}

export function getFront(
  result: ExpressionResult
): ObjectFront | LongStringFront | null {
  return isFront(result) ? result : null;
}
