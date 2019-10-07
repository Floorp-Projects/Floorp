/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { correctIndentation } from "./indentation";
import type { Expression } from "../types";

/*
 * wrap the expression input in a try/catch so that it can be safely
 * evaluated.
 *
 * NOTE: we add line after the expression to protect against comments.
 */
export function wrapExpression(input: string) {
  return correctIndentation(`
    try {
      ${input}
    } catch (e) {
      e
    }
  `);
}

function isUnavailable(value) {
  if (!value.preview || !value.preview.name) {
    return false;
  }

  return ["ReferenceError", "TypeError"].includes(value.preview.name);
}

export function getValue(expression: Expression) {
  const value = expression.value;
  if (!value) {
    return {
      path: expression.from,
      value: { unavailable: true },
    };
  }

  if (value.exception) {
    if (isUnavailable(value.exception)) {
      return { value: { unavailable: true } };
    }
    return {
      path: value.from,
      value: value.exception,
    };
  }

  if (value.error) {
    return {
      path: value.from,
      value: value.error,
    };
  }

  if (value.result && value.result.class == "Error") {
    const { name, message } = value.result.preview;
    if (isUnavailable(value.result)) {
      return { value: { unavailable: true } };
    }

    const newValue = `${name}: ${message}`;
    return { path: value.input, value: newValue };
  }

  if (typeof value.result == "object") {
    return {
      path: value.result.actor,
      value: value.result,
    };
  }

  return {
    path: value.input,
    value: value.result,
  };
}
