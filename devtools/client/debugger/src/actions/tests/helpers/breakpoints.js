/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export function mockPendingBreakpoint(overrides: Object = {}) {
  const { sourceUrl, line, column, condition, disabled, hidden } = overrides;
  return {
    location: {
      sourceId: "",
      sourceUrl: sourceUrl || "http://localhost:8000/examples/bar.js",
      line: line || 5,
      column: column || 1,
    },
    generatedLocation: {
      sourceUrl: sourceUrl || "http://localhost:8000/examples/bar.js",
      line: line || 5,
      column: column || 1,
    },
    astLocation: {
      name: undefined,
      offset: {
        line: line || 5,
      },
      index: 0,
    },
    options: {
      condition: condition || null,
      hidden: hidden || false,
    },
    disabled: disabled || false,
  };
}

export function generateBreakpoint(
  filename: string,
  line: number = 5,
  column: number = 0
) {
  return {
    id: "breakpoint",
    originalText: "",
    text: "",
    location: {
      sourceUrl: `http://localhost:8000/examples/${filename}`,
      sourceId: `${filename}`,
      line,
      column,
    },
    generatedLocation: {
      sourceUrl: `http://localhost:8000/examples/${filename}`,
      sourceId: filename,
      line,
      column,
    },
    astLocation: undefined,
    options: {
      condition: "",
      hidden: false,
    },
    disabled: false,
  };
}
