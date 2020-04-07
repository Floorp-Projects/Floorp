/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Why } from "../../../types";
import type { NamedValue } from "./types";

export function getFramePopVariables(why: Why, path: string): NamedValue[] {
  const vars: Array<NamedValue> = [];

  if (why && why.frameFinished) {
    const { frameFinished } = why;

    // Always display a `throw` property if present, even if it is falsy.
    if (Object.prototype.hasOwnProperty.call(frameFinished, "throw")) {
      vars.push({
        name: "<exception>",
        path: `${path}/<exception>`,
        contents: { value: frameFinished.throw },
      });
    }

    if (Object.prototype.hasOwnProperty.call(frameFinished, "return")) {
      const returned = frameFinished.return;

      // Do not display undefined. Do display falsy values like 0 and false. The
      // protocol grip for undefined is a JSON object: { type: "undefined" }.
      if (typeof returned !== "object" || returned.type !== "undefined") {
        vars.push({
          name: "<return>",
          path: `${path}/<return>`,
          contents: { value: returned },
        });
      }
    }
  }

  return vars;
}

export function getThisVariable(this_: any, path: string): ?NamedValue {
  if (!this_) {
    return null;
  }

  return {
    name: "<this>",
    path: `${path}/<this>`,
    contents: { value: this_ },
  };
}

// Get a string path for an scope item which can be used in different pauses for
// a thread.
export function getScopeItemPath(item: Object): string {
  // Calling toString() on item.path allows symbols to be handled.
  return item.path.toString();
}
