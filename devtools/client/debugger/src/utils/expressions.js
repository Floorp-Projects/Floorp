/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { correctIndentation } from "./indentation";
import { getGrip } from "./evaluation-result";
import type { Expression } from "../types";
import type { Grip } from "../client/firefox/types";

const UNAVAILABLE_GRIP = { unavailable: true };

/*
 * wrap the expression input in a try/catch so that it can be safely
 * evaluated.
 *
 * NOTE: we add line after the expression to protect against comments.
 */
export function wrapExpression(input: string): string {
  return correctIndentation(`
    try {
      ${input}
    } catch (e) {
      e
    }
  `);
}

function isUnavailable(value): boolean {
  if (!value.preview || !value.preview.name) {
    return false;
  }

  return ["ReferenceError", "TypeError"].includes(value.preview.name);
}

export function getValue(
  expression: Expression
): Grip | string | number | null | Object {
  const { value, exception, error } = expression;

  if (error) {
    return error;
  }

  if (!value) {
    return UNAVAILABLE_GRIP;
  }

  if (exception) {
    if (isUnavailable(exception)) {
      return UNAVAILABLE_GRIP;
    }
    return exception;
  }

  const valueGrip = getGrip(value.result);

  if (
    valueGrip &&
    typeof valueGrip === "object" &&
    valueGrip.class == "Error"
  ) {
    if (isUnavailable(valueGrip)) {
      return UNAVAILABLE_GRIP;
    }

    const { name, message } = valueGrip.preview;
    return `${name}: ${message}`;
  }

  return valueGrip;
}
