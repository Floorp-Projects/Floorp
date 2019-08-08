/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// Returns value to show to user from a GRIP
export function getValue(value: any) {
  if (typeof value !== "object") {
    return value;
  }

  if (value.type === "null") {
    return null;
  }
  if (value.type === "undefined") {
    return undefined;
  }
  const { preview } = value;
  if (preview) {
    const { kind } = preview;
    if (kind === "ArrayLike") {
      return preview.items || "...";
    }
    if (kind === "Object") {
      const object = {};
      const { ownProperties } = preview;
      Object.keys(ownProperties).forEach((key: string) => {
        object[key] = getValue(ownProperties[key].value);
      });
      return object;
    }
  }
  // For objects with objects inside it, preview isn't present for
  // second level objects
  return "...";
}
